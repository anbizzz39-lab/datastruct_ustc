#include "Building.h"
#include <algorithm>

Building::Building(int topfloor, int elvcount)
	: storeynum(topfloor), elevatornum(elvcount)
{
	//只在这里 resize 一次：Storey / elevator 持有裸指针节点且禁止拷贝，
	//所以容器必须一次到位，之后不再扩容/增删元素
	index.resize(static_cast<size_t>(storeynum) + 1);      //楼层 0 ~ storeynum
	floorstate.resize(static_cast<size_t>(storeynum) + 1);
	elva.resize(static_cast<size_t>(elevatornum));

	for (int f = 0; f <= storeynum; ++f) {
		index[f].index = f;                                //记下自己的楼层号
	}
	for (int e = 0; e < elevatornum; ++e) {
		elva[e].index = e;                                 //记下自己的编号
		elva[e].building = this;
	}
}

// ======================= 调度 =======================
// 每个 tick 被 main 调用一次（now = 当前仿真时刻）。
//
// ① 清空上一轮的认领，并按各层队列的实况重算呼叫状态（waiting / none）。
//
// ② 逐轮仲裁：所有 flexible 的电梯各自报出"我想去哪一层"（naturalNext）和到达时刻。
//    同一个目标层被多台梯抢 → 到达最早的那台胜出，立刻把该层的呼叫标成 coming
//    （表示"这一单归它了"）；落败的留到下一轮重新提议 —— 因为 check() 对"已经
//    coming 的呼叫"会返回"不归我"，它自然就换一个目标了。如果落败的那台自己本来
//    就要去那一层（比如车上有乘客在那儿下），wheretogo 照样返回同一层，那就两台
//    都去、互不影响。每一轮至少定下来一台，所以轮数有上界。
//
// ③ 剩下"没人顺路会去"的呼叫（最典型的是反方向的：空梯停在 1 楼、5 楼有人要下楼）：
//    让所有 flexible 的空梯一起比 etaTo()，谁最早到谁认领（设 target/targetDir + coming）。
//    正在下降回一楼待命的空梯也参与这个比较 —— 它的 etaTo 里已经含了"先降到一楼
//    停稳、再起步加速上来"的时间，所以和静止的梯比是公平的。
//
// ④ 什么也没抢到的电梯保持静止（target 一直是 -1），下个 tick 再议。
int Building::organize(int now) {
	// ① 清空认领 + 按队列实况重算呼叫状态
	for (int f = 0; f <= storeynum; ++f) {
		index[f].releaseAll();
		index[f].refreshstate();
	}
	for (elevator& cd : elva) {
		cd.target = -1;
		cd.targetDir = Idle;
	}

	// ② 逐轮仲裁
	std::vector<char> pending(static_cast<size_t>(elevatornum), 1);   //还没定下来的电梯

	for (int round = 0; round <= elevatornum; ++round) {
		// 本轮各家提议（只让"还没定下来"的 flexible 梯报）
		struct Prop { int floor; int eta; int el; };
		std::vector<Prop> props;
		for (elevator& cd : elva) {
			if (!pending[cd.index]) continue;
			if (cd.st1.flexible == false) {      //满载 / 正在开关门：不参与调度
				pending[cd.index] = 0;
				continue;
			}
			if (cd.time > now) {                 //正在途中：它报出来的只是"到了那边之后"的打算，
				pending[cd.index] = 0;           //  不能用它去占住别人现在就能接的呼叫
				continue;
			}
			const int f = cd.naturalNext();
			if (f == cd.floor) {                 //它没有要去的地方 → 静止
				pending[cd.index] = 0;
				continue;
			}
			props.push_back(Prop{ f, cd.etaTo(f), cd.index });
		}
		if (props.empty()) break;

		bool settledAny = false;
		for (size_t i = 0; i < props.size(); ++i) {
			if (!pending[props[i].el]) continue;

			// 同层冲突：到达时间最小的胜出（时间相同则编号小的胜出，保证确定性）
			bool win = true;
			for (size_t j = 0; j < props.size(); ++j) {
				if (i == j || props[j].floor != props[i].floor) continue;
				if (props[j].eta < props[i].eta ||
					(props[j].eta == props[i].eta && props[j].el < props[i].el)) {
					win = false;
					break;
				}
			}
			if (!win) continue;                  //落败：这一轮先不定，下一轮重新提议

			// 胜者定下来：认领这一层，并把"它会服务的那个方向"的呼叫标成 coming。
			// （naturalNext 报出来的层，一定是它行进方向上第一个该停的层，所以
			//   targetDir 就是它会服务的那个方向；反方向的呼叫先不认领，免得挡住别人顺路的单。）
			elevator& cd = elva[props[i].el];
			const int f = props[i].floor;
			cd.target = f;
			cd.targetDir = (f > cd.floor) ? GoingUP : GoingDown;
			if (index[f].hasCall(cd.targetDir)) {
				index[f].assign(cd.targetDir, cd.index);
			}
			pending[cd.index] = 0;
			settledAny = true;
		}
		if (!settledAny) break;                  //一轮下来谁也没定（都在等对手）→ 不空转
	}

	// ③ 没人顺路会去的呼叫：所有 flexible 的空梯一起比到达时间，最早的认领。
	//    配对顺序是"先救快等超时的人"（截止时刻早的先配），同一单内部再按到达时间取最快的那台 ——
	//    否则空梯只有一台时，贪心总先挑最近的那单，远处那单会一直轮空直到有人超时放弃。
	struct Cand { int deadline; int eta; int el; int floor; state dir; };
	std::vector<Cand> cands;
	const state dirs[2] = { GoingUP, GoingDown };

	for (int f = 0; f <= storeynum; ++f) {
		for (const state d : dirs) {
			if (!index[f].hasCall(d)) continue;                  //已经有人负责这一单了
			if (index[f].check(now, d, -1) != Highest) continue;  //现在还没人在等（人还没到）

			const int deadline = index[f].earliestDeadline(d);
			for (const elevator& cd : elva) {
				if (!cd.canClaim(f)) continue;                   //满载/车上有乘客/就在这层 → 不参与

				//它自己这一层现在就有人在等着上车 → 它马上就要先服务本层、计划当场就变了，
				//别把远处的单派给它，否则这一单会被静静地丢掉（认领了却没人去）
				if (index[cd.floor].check(now, Idle, cd.index) == Highest) continue;

				cands.push_back(Cand{ deadline, cd.etaTo(f), cd.index, f, d });
			}
		}
	}
	std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
		if (a.deadline != b.deadline) return a.deadline < b.deadline;   //先救最急的
		if (a.eta != b.eta) return a.eta < b.eta;                      //同一单里谁先到谁去
		return a.el < b.el;                                            //完全并列时按编号，保证确定性
	});

	std::vector<char> elUsed(static_cast<size_t>(elevatornum), 0);
	std::vector<char> callUsed(static_cast<size_t>(storeynum) * 2 + 2, 0);
	for (const Cand& c : cands) {
		const int callId = c.floor * 2 + ((c.dir == GoingUP) ? 0 : 1);
		if (elUsed[c.el] || callUsed[callId]) continue;   //一台梯只接一单，一单只给一台梯
		elUsed[c.el] = 1;
		callUsed[callId] = 1;

		elevator& cd = elva[c.el];
		cd.target = c.floor;
		cd.targetDir = c.dir;
		index[c.floor].assign(c.dir, c.el);
	}

	// ④ 其余电梯保持静止，下个 tick 再议
	return 0;
}

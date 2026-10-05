#include "Building.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

//一条乘客记录
struct Rec { int from, to, intime, waittime; };

// ================= 数据输入 =================
// 文件格式（空白分隔，'#' 之后当注释忽略）：
//     最高楼层号  电梯数
//     人数
//     起点层 目的层 到达时刻 最大等待时间      ← 重复"人数"次
//
// 人是一开始就全部插进队列的（预先入队），intime 表示"他将会出现的时刻"，
// 所以 check() 才能按未来时刻预测"我到的时候他还在不在等"。

static void loadDemo(int& topfloor, int& elvnum, std::vector<Rec>& recs) {
	topfloor = 10;
	elvnum = 2;
	//注意"最大等待时间"要大于最坏情况下"调梯 + 乘车"的耗时：
	//这台楼 10 层的单程行程就要 14 + 10*51 = 524（上行），给 600 太紧，
	//于是演示数据统一给 1200。想看"大面积放弃"的场景，把它调小即可。
	const int W = 1200;
	const Rec demo[] = {
		{ 0,  8,   0, W}, { 0,  5,  30, W}, { 3,  9,  60, W},
		{ 5,  1, 120, W}, { 7,  0, 150, W}, { 2, 10, 200, W},
		{ 9,  2, 260, W}, { 1,  6, 300, W}, { 4,  0, 380, W},
		{ 6,  9, 450, W}, { 8,  3, 520, W}, { 0,  7, 600, W},
	};
	for (const Rec& r : demo) recs.push_back(r);
}

static bool loadFile(const char* path, int& topfloor, int& elvnum, std::vector<Rec>& recs) {
	std::ifstream fin(path);
	if (!fin) {
		std::cout << "打不开输入文件：" << path << "\n";
		return false;
	}

	//把整份文件的数字都读进来（按行读，'#' 之后丢弃）
	std::vector<int> nums;
	std::string line;
	while (std::getline(fin, line)) {
		const size_t sharp = line.find('#');
		if (sharp != std::string::npos) line.erase(sharp);

		std::istringstream iss(line);
		int v;
		while (iss >> v) nums.push_back(v);
	}

	if (nums.size() < 3) {
		std::cout << "输入格式错误：至少要有『最高楼层号 电梯数 人数』三个数\n";
		return false;
	}

	topfloor = nums[0];
	elvnum = nums[1];
	const int count = nums[2];
	const int got = (static_cast<int>(nums.size()) - 3) / 4;
	if (count < 0 || got < count) {
		std::cout << "输入格式错误：声明了 " << count << " 人，但只给了 " << got << " 条记录\n";
		return false;
	}

	recs.clear();
	for (int i = 0; i < count; ++i) {
		Rec r;
		r.from     = nums[3 + i * 4 + 0];
		r.to       = nums[3 + i * 4 + 1];
		r.intime   = nums[3 + i * 4 + 2];
		r.waittime = nums[3 + i * 4 + 3];
		recs.push_back(r);
	}
	return true;
}

// ================= 仿真 =================
int main(int argc, char** argv) {
	int topfloor = 0;
	int elvnum = 0;
	std::vector<Rec> recs;

	if (argc > 1) {
		if (!loadFile(argv[1], topfloor, elvnum, recs)) return 1;
	}
	else {
		loadDemo(topfloor, elvnum, recs);
		std::cout << "（没给输入文件，用内置演示数据；也可以： elevator.exe data.txt）\n";
	}
	if (topfloor < 1 || elvnum < 1) {
		std::cout << "楼层号和电梯数都必须 >= 1\n";
		return 1;
	}

	Building b(topfloor, elvnum);

	// ---- 预先全部入队：一次把所有人放到各自楼层，intime 表示他将会出现的时刻 ----
	int total = 0;
	int bad = 0;
	for (const Rec& r : recs) {
		if (r.from < 0 || r.from > topfloor || r.to < 0 || r.to > topfloor || r.from == r.to) {
			++bad;                      //越界 / 同层的记录直接跳过
			continue;
		}
		People* p = new People();
		p->instorey = r.from;
		p->outstorey = r.to;
		p->intime = r.intime;
		p->waittime = r.waittime;
		b.index[r.from].insert(*p);     //insert 接管这个对象的所有权
		++total;
	}

	// ---- 仿真主循环 ----
	int now = 0;
	int served = 0;
	int abandoned = 0;
	while (now < MAXTIME) {
		// ① 楼层维护：清掉已经等超时放弃的人
		for (int f = 0; f <= b.storeynum; ++f) {
			b.index[f].refreshqueue(now);
		}

		// ② 调度：给各层的呼叫派梯，并把空梯派去接人
		b.organize(now);

		// ③ 每台电梯：忙完了就推进（停站服务 → 运动）
		for (elevator& e : b.elva) {
			if (e.time <= now) e.step(now);
		}

		// ④ 统计，所有人都处理完就提前结束
		served = 0;
		abandoned = 0;
		for (const elevator& e : b.elva) served += e.servedcount;
		for (const Storey& s : b.index)   abandoned += s.abandoned;
		if (served + abandoned >= total) break;

		++now;
	}

	// ---- 输出统计 ----
	int totalwait = 0;
	int maxwait = 0;
	int riding = 0;
	int waiting = 0;
	for (const elevator& e : b.elva) {
		totalwait += e.totalwait;
		if (e.maxwait > maxwait) maxwait = e.maxwait;
		riding += e.num;
	}
	for (const Storey& s : b.index) {
		for (const People* p = s.head->next; p != s.tail; p = p->next) ++waiting;
	}

	std::cout << "\n================ 仿真结果 ================\n";
	std::cout << "楼层 0~" << b.storeynum << "，电梯 " << b.elevatornum
	          << " 台，乘客 " << total << " 人";
	if (bad > 0) std::cout << "（另有 " << bad << " 条记录非法被跳过）";
	std::cout << "\n";
	std::cout << "仿真结束时刻 : " << now << "\n";
	std::cout << "已完成服务   : " << served << " 人\n";
	std::cout << "等超时放弃   : " << abandoned << " 人\n";
	std::cout << "仍在等待     : " << waiting << " 人\n";
	std::cout << "仍在轿厢     : " << riding << " 人\n";
	if (served > 0) {
		std::cout << std::fixed << std::setprecision(1);
		std::cout << "平均等待时间 : " << (static_cast<double>(totalwait) / served) << "\n";
		std::cout << "最长等待时间 : " << maxwait << "\n";
	}
	std::cout << "---- 各电梯 ----\n";
	for (const elevator& e : b.elva) {
		std::cout << "  梯" << e.index
		          << "  时钟=" << e.time
		          << "  所在层=" << e.floor
		          << "  载客=" << e.num
		          << "  服务=" << e.servedcount << "人"
		          << "  平均等待=";
		if (e.servedcount > 0) std::cout << (static_cast<double>(e.totalwait) / e.servedcount);
		else                   std::cout << "0.0";
		std::cout << "  最长等待=" << e.maxwait << "\n";
	}

	return 0;
}

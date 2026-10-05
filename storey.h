#pragma once
#include "people.h"
#include "enumdata.h"

class Storey {
public:
	Storeystate st;
	int index = 0;                 //本层的楼层号
	int abandoned = 0;             //等超时放弃的人数（统计用）
	People* head = nullptr;//按照到来时间排序,头部虚节点存储该层最新时间
	People* tail = nullptr;//最后一个人的位置后一个-虚尾

	Storey();                        //建立虚头虚尾
	~Storey();                       //释放本层残留的乘客与虚节点
	Storey(const Storey&) = delete;  //持有裸指针节点，禁止拷贝（否则两个对象共用同一对哨兵）
	Storey& operator=(const Storey&) = delete;
	Storey(Storey&& other) noexcept;
	Storey& operator=(Storey&& other) noexcept;

	bool empty() const { return head == nullptr || head->next == tail; }

	// t  ：询问的时刻（我预计到达 / 关门结束的时刻），不是"现在"
	// dir：我如果来会朝哪个方向走；Idle 表示还没定方向
	// me ：我的电梯编号，用来判断"这个呼叫是不是派给我的"
	// Highest：本层我这个方向有可用乘客，且呼叫没被别的电梯接走  → 值得停
	// Middle ：本层有人在等，但不归我这个方向（反方向的人 / 同向呼叫已被别的梯接走）
	// Lowest ：本层没有可用的人
	// 判据时刻 t 必须和调用方把 time 推进到的时刻一致，否则会出现
	// "检测到将来有人，但现在没人上得来"。
	precedence check(int t, state dir, int me) const;

	void refreshstate();
	void refreshqueue(int time);   //每 tick 调用：清掉已经等超时放弃的人

	// 接管 other 的所有权：other 必须是用 new 创建的，且调用后不再被引用
	void insert(People& other);    //按 intime 有序插入，不依赖输入顺序

	// ---- 调度（由 Building::organize 使用）----
	bool hasCall(state dir) const;            //这个方向有没有"还没派出去"的呼叫
	int  earliestDeadline(state dir) const;   //这个方向最急的那位乘客的截止时刻（没人等 = -1）
	void assign(state dir, int elindex);      //把这个方向的呼叫派给某台电梯
	void releaseAll();                        //解除本层所有派单
};

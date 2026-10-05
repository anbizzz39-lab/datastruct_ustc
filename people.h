#pragma once

// 链表节点（乘客）。
// 所有权约定：一旦进入 Storey 或 elevator 的队列，这个对象就归队列所有，
// 由队列在「到达目的层 / 等超时放弃 / 队列析构」时 delete。
// 所以 insert() 的实参必须是用 new 创建的，并且调用之后不要再引用它。
class People
{
public:
	int instorey = 0;   //进入楼层
	int outstorey = 0;  //目的楼层
	int intime = 0;     //开始排队时间（预先入队时 = 他将会出现的时刻）
	int waittime = 0;   //最大等待时间
	int boardtime = -1; //实际上车时刻（统计等待时间用）
	People* next = nullptr;   //用与形成链表
	People* pre = nullptr;
};

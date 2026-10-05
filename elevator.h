#pragma once
#include "people.h"
#include "enumdata.h"

class Storey;     // 只在参数里用引用
class Building;   // 只用一个指针


class elevator {
public:
	int index = 0;                     //电梯编号
	Building* building = nullptr;  //在那一栋楼里面
	int time = 0;   //存储elevator运行时间（与仿真时间同轴：动作完成时刻）
	int num = 0;    //用来存现在有多少人在电梯里面
	int floor = 0;   //用于记录所在楼层
	elstate st1;  //用于记录电梯的操作状态
	People* queue_head = nullptr;//按照出楼层从低到高排序-虚头
	People* queue_rear = nullptr;//高位-虚尾
	state st2 = Idle;  //用于记录电梯上行还是下行,只有没用当前state任务时再改变st2

	//调度器认领：认领了就直奔那一层（wheretogo 会优先返回它）；
	//到站后由 move() 清掉。targetDir 只用来记录"这一单是接哪个方向的呼叫"，
	//到站后真正往哪边走仍由 turnway() 依据实际乘客重新决定。
	int target = -1;        //认领的目标层（-1 = 没有认领）
	state targetDir = Idle; //认领的服务方向

	int idlesince = -1;     //从哪一刻开始在这层空闲（-1 = 不空闲）；"待满 MAX_WAIT 回一楼"用

	//统计
	int servedcount = 0;   //已送达目的层的乘客数
	int totalwait = 0;     //这些乘客的等待时间总和
	int maxwait = 0;       //其中最长的等待时间

	elevator();                        //建立虚头虚尾
	~elevator();                       //释放轿厢内残留的乘客与虚节点
	elevator(const elevator&) = delete;
	elevator& operator=(const elevator&) = delete;
	elevator(elevator&& other) noexcept;
	elevator& operator=(elevator&& other) noexcept;

	// ============ 电梯的两大部分 ============
	void step(int now);        //由 main 驱动：忙完了就做下一件事
	void waitleave();          //① 停站服务：下客 → 等晚到的人 → 上客 → 关门，并定好去向
	void move();               //② 运动：朝下一个该停的层走，消耗行程时间
	int  travelTimeTo(int target) const;         //从停站起步走一趟要多久
	int  etaTo(int f) const;                     //预计到达 f 层的时刻（含把手上这趟做完）
	bool willServe(state dir, int target) const; //走到 target 时会不会服务 dir 方向
	bool canClaim(int f) const;                  //调度器能不能把这个呼叫认领给我

	// ============ 查询 ============
	bool checkfloor(int floor) const;
	int judgeup() const;
	int judgedown() const;
	int wheretogo() const;                       //我这一趟要去哪层（认领优先）
	int naturalNext() const;                     //不被认领影响时，我自己会去的下一层

	// ============ 开关门与上下客 ============
	void turnway();
	void insertpeople(People& other);   //接管 other 的所有权

	void ensureopen();
	void out(Storey& people);
	void in(Storey& people);
	void holdclose();
	void close();

	void checkoutin();
	void elstatecheck();

};

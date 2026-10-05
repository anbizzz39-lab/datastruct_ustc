#include "elevator.h"
#include "Building.h"
#include <algorithm>
#include <cstdlib>

//把以 head 开头的整条链（含虚节点）释放掉
static void freeList(People* head) {
	while (head != nullptr) {
		People* next = head->next;
		delete head;
		head = next;
	}
}

// ---------------- 生命周期 ----------------

elevator::elevator() {
	queue_head = new People();
	queue_rear = new People();
	queue_head->pre = nullptr;    queue_head->next = queue_rear;
	queue_rear->pre = queue_head; queue_rear->next = nullptr;
}

elevator::~elevator() {
	freeList(queue_head);
	queue_head = nullptr;
	queue_rear = nullptr;
}

elevator::elevator(elevator&& other) noexcept
	: index(other.index), building(other.building), time(other.time), num(other.num),
	  floor(other.floor), st1(other.st1), queue_head(other.queue_head),
	  queue_rear(other.queue_rear), st2(other.st2), target(other.target),
	  targetDir(other.targetDir), idlesince(other.idlesince),
	  servedcount(other.servedcount), totalwait(other.totalwait), maxwait(other.maxwait) {
	other.queue_head = nullptr;
	other.queue_rear = nullptr;
}

elevator& elevator::operator=(elevator&& other) noexcept {
	if (this == &other) return *this;
	freeList(queue_head);
	index = other.index;
	building = other.building;
	time = other.time;
	num = other.num;
	floor = other.floor;
	st1 = other.st1;
	st2 = other.st2;
	target = other.target;
	targetDir = other.targetDir;
	idlesince = other.idlesince;
	servedcount = other.servedcount;
	totalwait = other.totalwait;
	maxwait = other.maxwait;
	queue_head = other.queue_head;
	queue_rear = other.queue_rear;
	other.queue_head = nullptr;
	other.queue_rear = nullptr;
	return *this;
}

// ---------------- 轿厢查询 ----------------

bool elevator::checkfloor(int floor) const {
	if (queue_head == nullptr) return false;
	for (const People* temp = queue_head->next; temp != queue_rear; temp = temp->next) {
		if (temp->outstorey == floor) {
			return true;
		}
	}

	return false;
}


// 只返回应停靠楼层；-1 表示上行无请求
// 一趟行程的时间 = 加速(ACCEL_TIME) + 每层匀速(UP_RUN) + 一次减速(UP_SLOW)，
// 必须和 travelTimeTo() 完全一致，否则"决策以为 X 刻能到、实际 Y 刻才到"。
int elevator::judgeup() const {
	int temptime = time + ACCEL_TIME + UP_SLOW;
	int tempfloor = floor;

	while (tempfloor <= building->storeynum) {
		precedence p = (building->index)[tempfloor].check(temptime, GoingUP, index);

		bool someoneGetsOff = checkfloor(tempfloor);
		bool hasWaiting = (p == Highest);
		bool willHaveSpace = (num< MAX_PEOPLE || someoneGetsOff);

		if (someoneGetsOff || (willHaveSpace && hasWaiting)) {
			return tempfloor;          // ★ 只返回楼层
		}
		temptime += UP_RUN;
		tempfloor += 1;
	}

	return -1;
}

// 只返回应停靠楼层；-1 表示下行无请求
int elevator::judgedown() const {
	int temptime = time + ACCEL_TIME + DOWN_SLOW;
	int tempfloor = floor;

	while (tempfloor >= 0) {
		precedence p = (building->index)[tempfloor].check(temptime, GoingDown, index);

		bool someoneGetsOff = checkfloor(tempfloor);
		bool hasWaiting = (p == Highest);
		bool willHaveSpace = (num < MAX_PEOPLE) || someoneGetsOff;

		if (someoneGetsOff || (willHaveSpace && hasWaiting)) {
			return tempfloor;
		}
		temptime += DOWN_RUN;
		tempfloor -= 1;
	}

	return -1;
}

//我这一趟要去哪一层：调度器认领了就直接去那一层，否则按需求自己找。
//纯查询：不改 time / floor / st2。
int elevator::wheretogo() const {
    if (target != -1) return target;    //调度器认领优先（认领就是"这一层归我了"）
    return this->naturalNext();
}

//不被认领影响时，我自己会去的下一层（沿途第一个该停的层；返回当前层 = 没地方要去）。
//注意：这里不能因为 flexible == false 就直接返回当前层 ——
//满载的电梯仍然必须把乘客送到目的层，那才是它"该去的地方"。
//（调度器要跳过满载梯，由 Building::organize 里的 flexible 判断负责。）
int elevator::naturalNext() const {
    state tempst2 = st2;      // 复制当前方向，试探用
    int alreadyturned = 0;
    int tempfloor;

    for (int tried = 0; tried < 2; tried++) {
        if (tempst2 == GoingUP) {
            tempfloor = judgeup();
            if (tempfloor == -1 && alreadyturned == 0) {
                alreadyturned = 1;
                tempst2 = GoingDown;      // 只改临时变量
            }
            else if (tempfloor == -1 && alreadyturned == 1) {
                return floor;             // 两边都没请求，返回当前层
            }
            else {
                return tempfloor;         // 只返回目标层
            }
        }
        else if (tempst2 == GoingDown) {
            tempfloor = judgedown();
            if (tempfloor == -1 && alreadyturned == 0) {
                alreadyturned = 1;
                tempst2 = GoingUP;
            }
            else if (tempfloor == -1 && alreadyturned == 1) {
                return floor;
            }
            else {
                return tempfloor;
            }
        }
        else if (tempst2 == Idle) {
            int up = judgeup();
            int down = judgedown();

            if (up == -1 && down == -1) {
                return floor;
            }
            if (up != -1 && down != -1) {
                int uptime = this->travelTimeTo(up);
                int downtime = this->travelTimeTo(down);
                return (uptime <= downtime) ? up : down;
            }
            else if (up != -1) {
                return up;
            }
            else {
                return down;
            }
        }
    }
    return floor;
}

void elevator::insertpeople(People& other) {
    People* temp = queue_head->next;
    while (temp!=queue_rear && temp->outstorey < other.outstorey) {
        temp = temp->next;
    }
    People* pre = temp->pre;    

    pre->next = &other;        
    other.pre = pre;            
    other.next = temp;        
    temp->pre = &other;        

}

void elevator::turnway() {
    st1.flexible = true;
    if (queue_head->next == queue_rear) {   //空轿厢：没有任务
        st2 = Idle;
        return; }

    
    if (num == MAX_PEOPLE || st1.open == true) {
        st1.flexible = false;
    }
    if (floor == building->storeynum || st2 == GoingUP && queue_rear->pre->outstorey < floor) {
        st2 = GoingDown;
        return;
    }
    if (floor == 0 || st2 == GoingDown && queue_head->next->outstorey > floor) {
        st2 = GoingUP;
        return;
    }
    //★ 兜底：队列非空但 st2 还是 Idle（例如空梯刚上完客），上面两条都不命中。
    //  这里必须给出一个方向，否则会带着乘客一直停在 Idle。
    if (st2 == Idle) {
        st2 = (queue_rear->pre->outstorey > floor) ? GoingUP : GoingDown;
    }

}


void elevator::ensureopen() {
    if (st1.open != true) {
        st1.open = true;//此时打开门
        st1.flexible = false; //此时电梯没空
        time += OPEN_TIME;
    }
    return;
}

void elevator::out(Storey& people){
    ensureopen();
    //out 部分（只用轿厢自己的队列，不需要楼层队列）
    People* temp = queue_head->next; //电梯遍历到的这个人

    while (temp != queue_rear) {
        if (temp->outstorey == floor) {
            People* toleave = temp;
            temp->pre->next = temp->next;
            temp->next->pre = temp->pre;
            temp = temp->next;

            //统计：从"开始排队"到"上车"等了多久
            if (toleave->boardtime >= 0) {
                const int w = toleave->boardtime - toleave->intime;
                totalwait += w;
                if (w > maxwait) maxwait = w;
                ++servedcount;
            }

            delete toleave;
            time += ENTER_TIME;
            num--;
            continue;
        }
        temp = temp->next;
    }



}

void elevator::in(Storey& people){
    ensureopen();

    People* stemp = people.head->next;
    while (num < MAX_PEOPLE && stemp != people.tail) {
        if ((st2 == Idle || (st2 == GoingUP && stemp->outstorey > floor) || (st2 == GoingDown && stemp->outstorey < floor)) && stemp->intime <= time) {
            time += ENTER_TIME;
            stemp->boardtime = time;      //记录上车时刻（统计等待时间用）
            People* next = stemp->next;   //先记住后继，摘链之后这个节点就归轿厢所有
            stemp->pre->next = stemp->next;
            stemp->next->pre = stemp->pre;
            this->insertpeople(*stemp);
            stemp = next;
            num++;
            continue;
        }
        stemp = stemp->next;
    }
    return;


}

void elevator::holdclose() {
    Storey* st = &(building->index)[floor];

    //上客分两种，顺序不能颠倒：
    //  ① 现在就站在门口等的人 —— 用"当前时刻"问，必须马上让他们上来。
    //     （如果只用 time+CHECK_TIME 问，那么"还差 20 tick 就等超时"的人会被漏掉，
    //       人明明就在门口，电梯却关门走了。）
    //  ② 这一层 CHECK_TIME 内还会到的人 —— 把时钟推过去等他。
    for (int guard = 0; num < MAX_PEOPLE && guard < 2 * MAX_PEOPLE; ++guard) {
        if (st->check(time, st2, index) == Highest) {
            const int before = num;
            this->in(*st);
            if (num != before) continue;      //上来人了，继续看还有没有
            break;                            //检测到却上不来 → 收手（防状态撒谎时卡死）
        }

        if (st->check(time + CHECK_TIME, st2, index) == Highest) {
            time += CHECK_TIME;               //推到"刚才问的那个时刻"，他就上得来了
            const int before = num;
            this->in(*st);
            if (num != before) continue;
        }
        break;
    }

    st1.open = false;   //准备关门

    return;
}

void elevator::close() {
    Storey* st = &(building->index)[floor];

    //① 先下客：有人要在这一层下就开门（满载也必须下）—— out() 内部会 ensureopen()，
    //  所以要先判断有没有人下，避免空停也开门计 OPEN_TIME
    if (this->checkfloor(floor)) this->out(*st);

    if (num == MAX_PEOPLE) {
        time += CLOSE_TIME;
        st1.open = false;
        st1.flexible = false;
        return;                                  //满载：上不了人，直接关门走
    }

    this->holdclose();                            //② 等晚到的人 + 上客（它结束时已关门）

    int currenttime = time;
    precedence a = st->check(currenttime + CLOSE_TIME, st2, index);   //关门这段时间里有人来吗
    while (num < MAX_PEOPLE && a == Highest) {
        time += CLOSE_TIME;                       //★ 推进到"刚才问的那个时刻" = 重新开门
        st1.open = true;                          //门重新打开（耗时已在上面计过）
        st1.flexible = false;

        int before = num;
        this->in(*st);                            //现在被检测到的人 intime ≤ time，上得来
        if (num == before) break;                 //没人上得来 → 收手

        this->holdclose();                        //重新走一遍"等一个 CHECK_TIME"

        currenttime = time;                       //判据时间跟着时钟走
        a = st->check(currenttime + CLOSE_TIME, st2, index);
    }

    time += CLOSE_TIME;
    st1.open = false;                             //兜底：任何出口，门都是关的
    return;
}

// ================= ① 停站服务 =================
// 停在当前层：下客 → 等一个 CHECK_TIME 内会到的人 → 上客 → 关门，然后重新判断去向。
// 这是一个"步"，不是阻塞循环：空闲时立刻把控制权交回 main，
// 让每个 tick 的 organize() 有机会把别的层的呼叫派过来。
void elevator::waitleave(){
    this->close();      //停站收尾（下客 → 等晚到者 + 上客 → 关门）
    this->turnway();    //服务完重新定方向
    if (st2 != Idle) {  //有活干了 → 交给运动部分出发
        idlesince = -1; //不空闲，空闲计时清零
        return;
    }

    //轿厢空了、也没有任务：从这一刻开始计"空闲时间"
    st1.flexible = true;                       //此时是空闲的
    if (idlesince < 0) idlesince = time;       //记下开始闲着的时刻

    Storey* st = &(building->index)[floor];
    if (st->check(time, st2, index) == Highest) {   //有人要坐（st2 是 Idle，方向不限）
        idlesince = -1;                  //接到人了，空闲计时清零
        this->in(*st);
        this->close();               //直接关门，可以走
        this->turnway();
        return;
    }

    //闲着待满 MAX_WAIT 还没人叫 → 回一楼待命（st2 置成下行，move() 会把它送到 0 层）。
    //原来这里是一个 while (waittime <= MAX_WAIT) 的阻塞循环：它会把电梯自己的时钟
    //推进 300 刻，等于把电梯冻在原地、organize 也没法派它去别的层。改成计时器之后
    //"等到 MAX_WAIT 才回去"和"随时可以被调度"这两个性质都保住了。
    //注意：这里不扣 ACCEL_TIME —— 加速是"真的开始走"时才付的，由 move() 里的
    //      travelTimeTo() 统一计，空转一下不该收费。
    if (floor > 0 && time - idlesince >= MAX_WAIT) {
        st2 = GoingDown;
    }
}

// ================= ② 运动 =================
// 从当前层走到"下一个该停的层"：消耗行程时间、更新 floor。
// 中间经过的层不停 —— judgeup/judgedown 返回的就是沿途第一个该停的层。
void elevator::move() {
    //认领的活只在"车是空的"时候才认：否则（比如停站服务刚上了客）必须按自己的
    //需求走，不然会带着乘客飞过他们自己的目的层。wheretogo() 就是"认领优先"的查询。
    int goal;
    if (num == 0) {
        goal = this->wheretogo();
    }
    else {
        target = -1;                        //作废这一轮已经没用的认领
        targetDir = Idle;
        goal = this->naturalNext();
    }

    //彻底没活干（空车 + 没有任务）：待满 MAX_WAIT 后 st2 已被置成下行 → 回一楼待命
    if (goal == floor && num == 0 && st2 == GoingDown && floor != 0) {
        goal = 0;
    }
    if (goal == floor) {                 //没有要去的地方
        target = -1;
        targetDir = Idle;
        return;
    }

    st2 = (goal > floor) ? GoingUP : GoingDown;   //行进方向就是到站后继续的方向
    time += this->travelTimeTo(goal);
    floor = goal;
    target = -1;                        //到站，认领完成
    targetDir = Idle;
    idlesince = -1;                     //动起来了，空闲计时清零
}

//从当前层到 target 的行程时间
//   = 加速 ACCEL_TIME（每次从停站起步都要付一次）
//   + 每层匀速 UP_RUN / DOWN_RUN
//   + 到站前一次减速 UP_SLOW / DOWN_SLOW
//judgeup/judgedown 的 ETA 和 Building::organize 的派单估算都调用它，保证三处一致。
int elevator::travelTimeTo(int target) const {
    if (target > floor) return ACCEL_TIME + UP_SLOW + (target - floor) * UP_RUN;
    if (target < floor) return ACCEL_TIME + DOWN_SLOW + (floor - target) * DOWN_RUN;
    return 0;
}

//这台电梯走到 target 层时，会不会服务 dir 方向？
bool elevator::willServe(state dir, int target) const {
    if (dir == Idle) return true;
    if (target == floor) return (st2 == Idle || st2 == dir);
    const state travel = (target > floor) ? GoingUP : GoingDown;   //从这儿过去的方向
    return travel == dir;
}

//预计到达 f 层的时刻（调度比较用）。
//time 就是"我手上这趟行程做完的时刻"，里面已经含了到站减速；所以再走一趟 =
//先把手上这趟做完（含它的减速）+ 起步加速 + 沿途匀速 + 到站减速。
//   · 正停着等活的电梯  ：time ≈ 现在 → 只额外付"起步加速 + 行程"
//   · 正空车下降回一楼的电梯：time 是它到一楼的时刻 → 它得先降下去停稳，
//     再起步加速上来接人，所以天生比"本来就在楼上/楼下的静止梯"晚 —— 比较是公平的
int elevator::etaTo(int f) const {
    if (f == floor) return time;
    return time + this->travelTimeTo(f);
}

//调度器能不能把这个呼叫认领给我：
//  满载 / 正在开关门 → 不行
//  轿厢里有乘客     → 不行（它有自己的目的地，改道会让乘客坐过站）
//  空车，但自己已经有要去的地方 → 不行（别把它的计划顶掉；它本来就会顺路去那儿）
//  空车、而且自己也没活干 → 可以（到了那一层按 Idle 处理，哪个方向都能接）
//  就在我这一层     → 不用认领（停站服务时自然会接）
bool elevator::canClaim(int f) const {
    if (st1.flexible == false) return false;
    if (num != 0) return false;
    if (f == floor) return false;
    if (this->naturalNext() != floor) return false;
    return true;
}

// ================= 驱动 =================
// 由 main 每个 tick 调用：电梯"忙完了"（time ≤ now）就做下一件事。
void elevator::step(int now) {
    if (time < now) time = now;   //同步时钟：空闲的时间不计入电梯自身的运行时间
    this->waitleave();            //① 停站服务
    this->move();                 //② 运动
}

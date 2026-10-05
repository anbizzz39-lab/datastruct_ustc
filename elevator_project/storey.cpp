#include "storey.h"

//把以 head 开头的整条链（含虚节点）释放掉
static void freeList(People* head) {
	while (head != nullptr) {
		People* next = head->next;
		delete head;
		head = next;
	}
}

// ---------------- 生命周期 ----------------

Storey::Storey() {
	head = new People();
	tail = new People();
	head->pre = nullptr;  head->next = tail;
	tail->pre = head;     tail->next = nullptr;
	head->intime = 0;     //头部虚节点存"该层最新时间"
}

Storey::~Storey() {
	freeList(head);
	head = nullptr;
	tail = nullptr;
}

Storey::Storey(Storey&& other) noexcept
	: st(other.st), index(other.index), abandoned(other.abandoned),
	  head(other.head), tail(other.tail) {
	other.head = nullptr;
	other.tail = nullptr;
}

Storey& Storey::operator=(Storey&& other) noexcept {
	if (this == &other) return *this;
	freeList(head);
	st = other.st;
	index = other.index;
	abandoned = other.abandoned;
	head = other.head;
	tail = other.tail;
	other.head = nullptr;
	other.tail = nullptr;
	return *this;
}

// ---------------- 队列维护 ----------------

void Storey::refreshqueue(int time) {
	if (head == nullptr) return;   //被 move 走的空壳
	head->intime = time;           //头部虚节点存"该层最新时间"
	if (empty()) return;

	People* temp = head->next;
	while (temp != tail) {
		if (temp->intime + temp->waittime < time) {   //等超时了，放弃
			People* left = temp;
			temp->pre->next = temp->next;
			temp->next->pre = temp->pre;
			temp = temp->next;
			delete left;
			++abandoned;
			continue;
		}
		temp = temp->next;
	}
}

void Storey::insert(People& other) {
	if (other.outstorey == other.instorey) return;   // 同层不用坐

	// 按到达时间有序插入，维持"队列按 intime 递增"这个不变式
	People* p = head->next;
	while (p != tail && p->intime <= other.intime) p = p->next;

	other.pre = p->pre;
	other.next = p;
	p->pre->next = &other;     // ★ 双向链表插入要改 4 条指针，原来缺的就是这一条
	p->pre = &other;

	// 呼叫状态：waiting/none 只是给调度器看的提示（方向判断由 check 从乘客自身推导），
	// 只有 coming 有硬作用，所以这里只在 none 时升级成 waiting，绝不覆盖 coming
	if (other.outstorey > other.instorey && st.up_state == none) {
		st.up_state = waiting;
	}
	else if (other.outstorey < other.instorey && st.down_state == none) {
		st.down_state = waiting;
	}
}

// ---------------- 呼叫查询 ----------------

precedence Storey::check(int t, state dir, int me) const {
	if (head == nullptr) return Lowest;   //被 move 走的空壳

	// 我这个方向的呼叫是不是被"别的"电梯接走了
	const bool takenByOther =
		(dir == GoingUP && st.up_state == coming && st.up_el != me) ||
		(dir == GoingDown && st.down_state == coming && st.down_el != me);

	bool same = false;   // t 时刻有同向的人在等
	bool oppo = false;   // t 时刻有反向的人在等

	for (const People* p = head->next; p != tail; p = p->next) {
		if (p->intime > t) continue;                  // 到 t 时刻还没到
		if (p->intime + p->waittime < t) continue;    // 到 t 时刻已经等超时走了

		bool up = (p->outstorey > p->instorey);       // 同层内不会相等（insert 已排除）

		if (dir == GoingUP) { if (up) same = true; else oppo = true; }
		else if (dir == GoingDown) { if (up) oppo = true; else same = true; }
		else { same = true; }                         // 空闲梯：哪个方向都能上

		if (same && oppo) break;                      // 该知道的都知道了
	}

	if (same && !takenByOther) return Highest;        // 同向有人、没人抢 → 停
	if (same || oppo)          return Middle;         // 有人，但轮不到我这个方向
	return Lowest;                                    // 没人
}

void Storey::refreshstate() {//只有outin之后再用
	if (head == nullptr) return;

	bool up = false;
	bool down = false;
	for (const People* p = head->next; p != tail && !(up && down); p = p->next) {
		if (p->outstorey > p->instorey) {          //相等的没有进入
			up = true;
		}
		else if (p->outstorey < p->instorey) {
			down = true;
		}
	}

	// coming 表示"呼叫已经派给某台电梯了"，它无法从队列推导，不能被这里覆盖掉
	if (st.up_state != coming)   st.up_state = up ? waiting : none;
	if (st.down_state != coming) st.down_state = down ? waiting : none;
}

// ---------------- 调度辅助 ----------------

bool Storey::hasCall(state dir) const {
	return (dir == GoingUP) ? (st.up_state == waiting) : (st.down_state == waiting);
}

//这一层、这个方向上"最急的那位"的截止时刻（= intime + waittime）；没人在等就返回 -1。
//调度器拿它做"先救快等超时的人"：不然空梯只有一台时，贪心总是先挑最近的那单，
//远处那单会一直轮空、直到有人等超时放弃。
int Storey::earliestDeadline(state dir) const {
	if (head == nullptr) return -1;

	int best = -1;
	for (const People* p = head->next; p != tail; p = p->next) {
		const bool up = (p->outstorey > p->instorey);
		if (dir == GoingUP && !up) continue;
		if (dir == GoingDown && up) continue;

		const int deadline = p->intime + p->waittime;
		if (best < 0 || deadline < best) best = deadline;
	}
	return best;
}

void Storey::assign(state dir, int elindex) {
	if (dir == GoingUP) {
		st.up_state = coming;
		st.up_el = elindex;
	}
	else if (dir == GoingDown) {
		st.down_state = coming;
		st.down_el = elindex;
	}
}

void Storey::releaseAll() {
	if (st.up_state == coming) {
		st.up_state = none;
		st.up_el = -1;
	}
	if (st.down_state == coming) {
		st.down_state = none;
		st.down_el = -1;
	}
}

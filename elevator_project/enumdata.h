#pragma once

//电梯常数
constexpr int MAXTIME = 10000;
constexpr int MINTIME = 500;
constexpr double TIME = 0.1;  // 时间步长
constexpr int    CHECK_TIME = 40;   // 检查时间
constexpr int    OPEN_TIME = 20;   // 开门时间
constexpr int    CLOSE_TIME = 20;   // 关门时间
constexpr int    ENTER_TIME = 25;   // 一个乘客进出电梯的时间
constexpr int    MAX_WAIT = 300;  // 最大等待时间
constexpr int    ACCEL_TIME = 15;   // 加速时间
constexpr int    UP_RUN = 51;   // 上行匀速时间
constexpr int    UP_SLOW = 14;   // 上行减速时间
constexpr int    DOWN_RUN = 61;   // 下行匀速时间
constexpr int    DOWN_SLOW = 23;   // 下行减速时间
constexpr int    MAX_PEOPLE = 15;   // 电梯最大人数


enum state { GoingUP, GoingDown, Idle };
enum precedence{Lowest,Middle,Highest};

// 每层、每个方向的呼叫状态
enum CallState {
    none,      // 没人
    waiting,   // 有人，还没派梯
    coming     // 已经派给某台电梯了（这个位无法从队列推导出来，必须持久保存）
};

class Storeystate {
public:
    CallState up_state = none;      // 上行呼叫
    CallState down_state = none;    // 下行呼叫
    int up_el = -1;                 // 上行的呼叫派给了哪台电梯（-1 = 没派）
    int down_el = -1;               // 下行的呼叫派给了哪台电梯（-1 = 没派）
};

class elstate {
public:
    bool open = false;         //只有正在出人或者那段时间，open=true
    bool flexible = true;      //只有满载或者正在出入的电梯flexible=false
};

class waitingel {
public:
    int gotime = 0;
    int elindex = 0;
};

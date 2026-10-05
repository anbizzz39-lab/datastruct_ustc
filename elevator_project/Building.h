#pragma once
#include "storey.h"
#include "elevator.h"
#include <vector>

class Building {
public:
	// storeynum 是"最高楼层号"：楼层编号为 0 ~ storeynum，共 storeynum+1 层。
	// 电梯里的循环写的是 tempfloor <= building->storeynum，所以把它当成"层数"用会越界。
	const int storeynum;
	const int elevatornum;
	std::vector<Storeystate> floorstate;
	std::vector<elevator>    elva;
	std::vector<Storey>      index;

	// topfloor：最高楼层号（层数 = topfloor + 1）；elvcount：电梯台数
	Building(int topfloor, int elvcount);

	// 调度：每个 tick 调一次（now = 当前仿真时刻）
	//   ① 清空上轮认领 + 按各层队列实况重算呼叫状态
	//   ② 仲裁：所有 flexible 电梯各自报一个想去的楼层，同一层冲突时到达最早的胜出，
	//      落败的下一轮重新提议（已标 coming 的呼叫对别人就不再是"该我停"）
	//   ③ 没人顺路会去的呼叫（典型是反方向的），让所有 flexible 的空梯比到达时间抢
	//   ④ 什么也没抢到的保持静止
	int organize(int now);
};

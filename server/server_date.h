#pragma once

/*
 * ServerDate - Minimal Date implementation for headless server
 * No dependencies on graphics or game systems
 */

#include <cstdio>
#include <chrono>

namespace Server {

class ServerDate {
protected:
	int second;
	int minute;
	int hour;
	int day;
	int month;
	int year;

	std::chrono::steady_clock::time_point previousUpdate;
	bool updateme;

public:
	ServerDate();
	ServerDate(int newsecond, int newminute, int newhour, int newday, int newmonth, int newyear);
	~ServerDate() = default;

	void SetDate(int newsecond, int newminute, int newhour, int newday, int newmonth, int newyear);
	void SetDate(ServerDate* copydate);

	void Activate();
	void DeActivate();

	void AdvanceSecond(int n);
	void AdvanceMinute(int n);
	void AdvanceHour(int n);
	void AdvanceDay(int n);
	void AdvanceMonth(int n);
	void AdvanceYear(int n);

	int GetSecond() const { return second; }
	int GetMinute() const { return minute; }
	int GetHour() const { return hour; }
	int GetDay() const { return day; }
	int GetMonth() const { return month; }
	int GetYear() const { return year; }

	static const char* GetMonthName(int month);
	char* GetLongString();

	void Update();
};

} // namespace Server

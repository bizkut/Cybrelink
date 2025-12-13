/*
 * ServerDate - Minimal Date implementation for headless server
 */

#include "server_date.h"
#include <cstring>

namespace Server {

static const char* monthname[] = { "January", "February", "March",	   "April",	  "May",	  "June",
								   "July",	  "August",	  "September", "October", "November", "December" };

static char tempdate[64]; // Used to return date strings

ServerDate::ServerDate() :
	second(1),
	minute(1),
	hour(1),
	day(1),
	month(1),
	year(1000),
	updateme(false)
{
	previousUpdate = std::chrono::steady_clock::now();
}

ServerDate::ServerDate(int newsecond, int newminute, int newhour, int newday, int newmonth, int newyear) :
	updateme(false)
{
	SetDate(newsecond, newminute, newhour, newday, newmonth, newyear);
	previousUpdate = std::chrono::steady_clock::now();
}

void ServerDate::SetDate(ServerDate* copydate)
{
	if (copydate) {
		SetDate(copydate->GetSecond(),
				copydate->GetMinute(),
				copydate->GetHour(),
				copydate->GetDay(),
				copydate->GetMonth(),
				copydate->GetYear());
	}
}

void ServerDate::SetDate(int newsecond, int newminute, int newhour, int newday, int newmonth, int newyear)
{
	second = newsecond;
	minute = newminute;
	hour = newhour;
	day = newday;
	month = newmonth;
	year = newyear;

	AdvanceSecond(0); // Roll over any invalid times
}

void ServerDate::Activate() { updateme = true; }
void ServerDate::DeActivate() { updateme = false; }

void ServerDate::AdvanceSecond(int n)
{
	second += n;

	if (second > 59) {
		int numminutes = 1 + (second - 60) / 60;
		second = second % 60;
		AdvanceMinute(numminutes);
	}

	if (second < 0) {
		int numminutes = 1 + (-second - 1) / 60;
		second = second + (60 * numminutes);
		AdvanceMinute(-numminutes);
	}
}

void ServerDate::AdvanceMinute(int n)
{
	minute += n;

	if (minute > 59) {
		int numhours = 1 + (minute - 60) / 60;
		minute = minute % 60;
		AdvanceHour(numhours);
	}

	if (minute < 0) {
		int numhours = 1 + (-minute - 1) / 60;
		minute = minute + (60 * numhours);
		AdvanceHour(-numhours);
	}
}

void ServerDate::AdvanceHour(int n)
{
	hour += n;

	if (hour > 23) {
		int numdays = 1 + (hour - 24) / 24;
		hour = hour % 24;
		AdvanceDay(numdays);
	}

	if (hour < 0) {
		int numdays = 1 + (-hour - 1) / 24;
		hour = hour + (24 * numdays);
		AdvanceDay(-numdays);
	}
}

void ServerDate::AdvanceDay(int n)
{
	day += n;

	if (day > 30) {
		int nummonths = 1 + (day - 30) / 30;
		day = day % 30;
		if (day == 0) {
			day = 30;
		}
		AdvanceMonth(nummonths);
	}

	if (day < 1) {
		int nummonths = 1 + (-day) / 30;
		day = day + (30 * nummonths);
		AdvanceMonth(-nummonths);
	}
}

void ServerDate::AdvanceMonth(int n)
{
	month += n;

	if (month > 12) {
		int numyears = 1 + (month - 12) / 12;
		month = ((month - 1) % 12) + 1;
		AdvanceYear(numyears);
	}

	if (month < 1) {
		int numyears = 1 + (-month) / 12;
		month = month + (12 * numyears);
		AdvanceYear(-numyears);
	}
}

void ServerDate::AdvanceYear(int n) { year += n; }

const char* ServerDate::GetMonthName(int month)
{
	if (month > 0 && month <= 12) {
		return monthname[month - 1];
	}
	return "Unknown";
}

char* ServerDate::GetLongString()
{
	snprintf(tempdate,
			 sizeof(tempdate),
			 "%.2d:%.2d.%.2d, %d %s %d",
			 hour,
			 minute,
			 second,
			 day,
			 GetMonthName(month),
			 year);
	return tempdate;
}

void ServerDate::Update()
{
	if (!updateme) {
		return;
	}

	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previousUpdate).count();

	// Normal speed: 1 real second = 1 game second
	if (elapsed >= 1000) {
		AdvanceSecond(1);
		previousUpdate = now;
	}
}

} // namespace Server

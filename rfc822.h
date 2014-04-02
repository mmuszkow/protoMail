#pragma once

#include "stdinc.h"

namespace protoMail {
	static const char DAYS_NAMES[7][4] = {
		"Mon", 
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat",
		"Sun"
	};

	static const char MONTH_NAMES[12][4] = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec"
	};

	typedef struct timezone_diff {
		char	name[4];
		int		diff;
	} timezone_diff;

	static const int TIMEZONES_LEN = 35;

	static const timezone_diff TIMEZONES[TIMEZONES_LEN] = {
		{ "A", 1 },
		{ "B", 2 },
		{ "C", 3 },
		{ "D", 4 },
		{ "E", 5 },
		{ "F", 6 },
		{ "G", 7 },
		{ "H", 8 },
		{ "I", 9 },
		{ "K", 10 },
		{ "L", 11 },
		{ "M", 12 },
		{ "N", -1 },
		{ "O", -2 },
		{ "P", -3 },
		{ "Q", -4 },
		{ "R", -5 },
		{ "S", -6 },
		{ "T", -7 },
		{ "U", -8 },
		{ "V", -9 },
		{ "W", -10 },
		{ "X", -11 },
		{ "Y", -12 },
		{ "Z", 0 },
		{ "UT", 0 },
		{ "GMT", 0 },
		{ "EST", 5 },
		{ "EDT", 4 },
		{ "CST", 6 },
		{ "CDT", 5 },
		{ "MST", 7 },
		{ "MDT", 6 },
		{ "PST", 8 },
		{ "PDT", 7 }
	};

	static time_t parse_RFC822_Date(const char* str) {
		char*		tmp;
		char*		timezone;
		char		copy[32];
		size_t		length;
		struct tm	timeinfo;
		int			i;
		size_t		day_offset, year_offset;
		time_t		timezone_diff;

		if(!str) return 0;

		/* strip day name, useless */
		for(i=0; i<7; i++) {
			/* double parentheses to avoid compiler warning */
			if((tmp = (char*)strstr(str, DAYS_NAMES[i])))
				break;
		}
		if(tmp) tmp += 3; else tmp = (char*)str;
		if(tmp[0] == ',') tmp++;
		if(tmp[0] == ' ') tmp++;

		if(strlen(tmp) > 31) return 0;

		strcpy_s(copy, 31, tmp);
		length = strlen(copy);

		if(length < 20)	return 0;

		day_offset = 0;
		if(copy[1] == ' ') /* 1 digit in day, not in standard but... */
			day_offset = -1;
		copy[2+day_offset] = 0; /* day */
		copy[6+day_offset] = 0; /* named month */
		
		year_offset = 0;
		if(copy[9+day_offset] != ' ') /* 2 or 4 digit year, not in standard but very popular */
			year_offset = 2;
		copy[9+day_offset+year_offset] = 0; /* year */
		copy[12+day_offset+year_offset] = 0; /* hour */
		copy[15+day_offset+year_offset] = 0; /* min */
		copy[18+day_offset+year_offset] = 0; /* sec */

		memset(&timeinfo, 0, sizeof(struct tm));
		timeinfo.tm_mday = atoi(&copy[0]);
		
		/* parsing month name */
		for(i=0; i<12; i++) {
			if(strcmp(&copy[3+day_offset], MONTH_NAMES[i])==0) {
				timeinfo.tm_mon = i;
				break;
			}
		}
		/* wrong month name */
		if(i == 12) return 0;

		timeinfo.tm_year = atoi(&copy[7+day_offset]) - 1900;
		timeinfo.tm_hour = atoi(&copy[10+day_offset+year_offset]);
		timeinfo.tm_min = atoi(&copy[13+day_offset+year_offset]);
		timeinfo.tm_sec = atoi(&copy[16+day_offset+year_offset]);

		timezone_diff = 0;
		if(length > 19 + day_offset + year_offset) {
			timezone = &(copy[19+day_offset+year_offset]);

			if(timezone[0] == '+' || timezone[0] == '-') {
				if(length >= 22 + day_offset + year_offset) {
					char	hour[3], min[3];
					int		dh, dm;
					hour[0] = timezone[1]; hour[1] = timezone[2]; hour[2] = 0;
					min[0] = timezone[3]; min[1] = timezone[4]; min[2] = 0;
					dh = atoi(hour);
					dm = atoi(min);
					timezone_diff = dh * 3600 + dm * 60;
					if(timezone[0] == '-') timezone_diff = -timezone_diff;
				}
			}
			else {
				/* named difference */
				for(i=0; i<TIMEZONES_LEN; i++) {
					if(strcmp(timezone, TIMEZONES[i].name) == 0) {
						timezone_diff = TIMEZONES[i].diff * 3600;
						break;
					}
				}
				if(i == TIMEZONES_LEN) return 0;
			}
		}

		return mktime(&timeinfo) + timezone_diff;
	}
};
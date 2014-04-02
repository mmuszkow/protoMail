#pragma once

#include "stdinc.h"
#include "PluginController.h"
#include "rfc822.h"

namespace protoMail {
	class pop3Mail {
		std::string	from;
		std::string	subject;
		time_t		time;

		static std::string parsePop3(const std::string& str) {
			size_t start, end, encEnd;

			start = str.find("=?");
			if(start == std::string::npos)
				return str;

			end = str.find("?=");
			if(end == std::string::npos)
				return str;

			encEnd = str.find("?b?");
			if(encEnd == std::string::npos)
				encEnd = str.find("?B?");

			if(encEnd == std::string::npos)
				return str;

			// format is ok, let's extract encoding and base64
			std::string encoding = toLower(str.substr(start+2, encEnd-start-2));
			std::string base64 = str.substr(encEnd+3, end-encEnd-3);

			if(encoding == "utf-8")
				return base64decodeToStr(base64);

			if(encoding == "iso-8859-2") {
				std::string iso = base64decodeToStr(base64);
				return convertEnc(iso, 28592, CP_UTF8);
			}

			if(encoding == "iso-8859-1") {
				std::string iso = base64decodeToStr(base64);
				return convertEnc(iso, 28591, CP_UTF8);
			}

			if(encoding == "windows-1250") {
				std::string iso = base64decodeToStr(base64);
				return convertEnc(iso, 1250, CP_UTF8);
			}

			if(encoding == "windows-1250") {
				std::string iso = base64decodeToStr(base64);
				return convertEnc(iso, 1252, CP_UTF8);
			}

			// not encoded or unsupported encoding
			return str;
		}
	public:
		pop3Mail(const std::string& raw) {
			time = 0;
			std::vector<std::string> lines = explode(raw);
			unsigned int len = lines.size();
			bool parsed[3] = {false, false, false};
			for(unsigned int i=0; i<len; i++) {
				if(startsWith(lines[i], "From: ")) {
					from = parsePop3(lines[i].substr(6));
					parsed[0] = true;
				} else if(startsWith(lines[i], "Subject: ")) {
					subject = parsePop3(lines[i].substr(9));
					parsed[1] = true;
				} else if(startsWith(lines[i], "Date: ")) {
					std::string timeStr = lines[i].substr(6);
					time = parse_RFC822_Date(timeStr.c_str());
					if(time == 0) { // parsing error
						WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
						__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Parsing RFC822 date failed, date=%s", 
							convertEnc(timeStr).c_str());
					}
					parsed[2] = true;
				}
				if(parsed[0] && parsed[1] && parsed[2]) break; // parsed all the needed info
			}
		}

		inline const std::string& getSender() const {
			return const_cast<const std::string&>(from);
		}

		inline const std::string& getSubject() const {
			return const_cast<const std::string&>(subject);
		}

		inline time_t getTime() const {
			return time;
		}
	};
};

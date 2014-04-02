#include "stdinc.h"

#include "Smtp.h"

namespace protoMail {
	// recv and send with
	bool Smtp::checkReceive(Smtp* smtp, const char* buff, int len, FUNC onsucc, const char* errstr, bool accept3xx) {
		if(len > 0 && (buff[0] == '2' || (accept3xx && buff[0] == '3'))) {
			if(onsucc) onsucc(smtp);
			return true;
		} else {
			smtp->failed = true;			
			smtp->disconnect();
			if(len > 0) {
				std::string errBuff = errstr;
				errBuff += buff;
				smtp->fail(smtp, errBuff);
			} else
				smtp->fail(smtp, "SMTP - Received empty buffer");
			return false;
		}
	}

	bool Smtp::checkSend(Smtp* smtp, const char* fmt, ...) {
		va_list ap;
		
		va_start(ap, fmt);
		int len = _vscprintf(fmt, ap);
		char* tmp = new char[len+1];
		vsprintf_s(tmp, len + 1, fmt, ap);
		va_end(ap);
		len = strlen(tmp);

		if(SUCCEEDED(smtp->sock->send(tmp, len))) {
			logProtoConsole(smtp->wtw, smtp->netId, tmp, len, true);
			delete [] tmp;
			smtp->lastTimeOutCheck = GetTickCount();
			return true;
		} else {
			smtp->failed = true;		
			smtp->disconnect();
			delete [] tmp;
			smtp->fail(smtp, "SMTP - Send failed");
			return false;
		}
	}

	bool Smtp::is7bit(const std::string& str) {
		unsigned int i, len = str.size();
		const char* cstr = str.c_str();
		for(i=0; i<len; i++)
			if(cstr[i] & 0x80) return false;
		return true;
	}

	/*char Smtp::SPECIALS[SPECIALS_LEN] = {
		'.', '=', '?', ',', '(', ')', ':', '<', 
		'>', '@', ';', '\\', '"', '/', '[', ']' };

	bool Smtp::containsSpecial(const std::string& str) {
		// search for special chars
		bool contains = false;
		unsigned int len = str.size();
		for(unsigned int i=0; i<len; i++) {
			if(contains) break;

			if(str[i] < 0x20 || str[i] > 0x7E) {
				contains = true;
				break;
			}

			for(unsigned int j=0; j<SPECIALS_LEN; j++) {
				if(str[i] == SPECIALS[j]) {
					contains = true;
					break;
				}
			}
		}
		return contains;
	}*/

	std::string Smtp::encodeSmtp(const std::string& utf8) {
		if(utf8.size() == 0) return utf8;

		// no non ANSI letters, keep it plain
		if(is7bit(utf8)) return utf8;

		// encode to format =?UTF-8?B?base64(subject)?=
		std::ostringstream encoded;
		encoded << "=?UTF-8?B?" << base64encode(utf8) << "?=";
		return encoded.str();
	}

	const char* Smtp::MIME_TYPES[MIME_TYPES_LEN][2] = {
		{ "avi", "video/msvideo, video/avi, video/x-msvideo" },
		{ "bmp", "image/bmp" },
		{ "bz2", "application/x-bzip2" },
		{ "css", "text/css" },
		{ "dtd", "application/xml-dtd" },
		{ "doc", "application/msword" },
		{ "docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document" },
		{ "exe", "application/octet-stream" },
		{ "gif", "image/gif" },
		{ "gz", "application/x-gzip" },
		{ "htm", "text/html" },
		{ "html", "text/html" },
		{ "jar", "application/java-archive" },
		{ "jpg", "image/jpeg" },
		{ "jpeg", "image/jpeg" },
		{ "js", "application/x-javascript" },
		{ "mp3", "audio/mpeg" },
		{ "mpg", "video/mpeg" },
		{ "mpeg", "video/mpeg" },
		{ "ogg", "audio/vorbis, application/ogg" },
		{ "pdf", "application/pdf" },
		{ "pl", "application/x-perl" },
		{ "png", "image/png" },
		{ "ppt", "application/vnd.ms-powerpoint" },
		{ "ps", "application/postscript" },
		{ "rar", "application/rar" },
		{ "qt", "video/quicktime" },
		{ "rtf", "application/rtf" },
		{ "svg", "image/svg+xml" },
		{ "swf", "application/x-shockwave-flash" },
		{ "tar.gz", "application/x-tar" },
		{ "tgz", "application/x-tar" },
		{ "tif", "image/tiff" },
		{ "tiff", "image/tiff" },
		{ "txt", "text/plain" },
		{ "wav", "audio/wav, audio/x-wav" },
		{ "xls", "application/vnd.ms-excel" },
		{ "xml", "application/xml" },
		{ "zip", "application/zip, application/x-compressed-zip" }
	};

	const char* Smtp::getMimeTypeFromExt(const std::string& filepath) {
		size_t dotPos = filepath.find_last_of('.');
		if(dotPos == std::string::npos)
			return "application/octet-stream";

		std::string ext = toLower(filepath.substr(dotPos + 1));

		for(unsigned int i=0; i<MIME_TYPES_LEN; i++)
			if(ext == MIME_TYPES[i][0]) 
				return MIME_TYPES[i][1];
		
		return "application/octet-stream";
	}

	void Smtp::sendMailData(Smtp* smtp) {
		smtpMail& msg = smtp->message;

		// ustawienie czasu zgodnego z RFC822 (GMT)
		SYSTEMTIME localTime = {0};
		GetSystemTime(&localTime);

		TIME_ZONE_INFORMATION timeZoneInfo;
		if(GetTimeZoneInformation(&timeZoneInfo) == TIME_ZONE_ID_INVALID) {
			smtp->failed = true;
			smtp->disconnect();
			smtp->fail(smtp, "SMTP - Could not determine local timezone");
			return;
		}

		SYSTEMTIME gmtTime = {0};
		if(TzSpecificLocalTimeToSystemTime(&timeZoneInfo, &localTime, &gmtTime) == FALSE) {
			smtp->failed = true;
			smtp->disconnect();
			smtp->fail(smtp, "SMTP - Could not convert local time to UTC time");
			return;
		}

		char timeStr[64];
		float timeZoneDiff = -( float() / 60 );
		sprintf_s(timeStr, 63, "%d %s %.4d %.2d:%.2d:%.2d %+.2d%.2d", 
			gmtTime.wDay,
			MONTH_NAMES[gmtTime.wMonth-1],
			gmtTime.wYear,
			gmtTime.wHour,
			gmtTime.wMinute,
			gmtTime.wSecond,
			-timeZoneInfo.Bias/60,
			abs(timeZoneInfo.Bias)%60);

		std::ostringstream content;
		content <<	"MIME-Version: 1.0\r\n"
					"From: <" << smtp->mail << ">\r\n"
					"To: " << encodeSmtp(msg.recipentName) << " <" << msg.getRecipentsConcated() << ">\r\n"
					"Subject: " << encodeSmtp(msg.subject) << "\r\n"
					"Date: " << timeStr << "\r\n";

		unsigned int attachments = msg.attachments.size();

		if(attachments > 0)
			content <<	"Content-Type: multipart/mixed; boundary=\"--boundary_protoMail\""
						"\r\n"
						"----boundary_protoMail\r\n";

		content << "Content-Type: text/plain; charset=UTF-8\r\n";
		if(is7bit(msg.message)) 
			content <<	"\r\n" << msg.message << "\r\n";
		else
			content <<	"Content-Transfer-Encoding: base64\r\n" <<
						"\r\n" << base64encode(msg.message) << "\r\n";

		for(unsigned int i=0; i<attachments; i++) {
			FILE* f;
			size_t read;
			char readBuff[16384];
			char baseBuff[21854];

			fopen_s(&f, msg.attachments[i].c_str(), "rb");
			if(f) {
				base64_encodestate state;
				UINT_PTR baseLen;

				// send attachment "header"
				content <<	"----boundary_protoMail\r\n"
							"Content-Type: " << getMimeTypeFromExt(msg.attachments[i]) << 
							"; name=\"" << extractFilename(msg.attachments[i]) << "\"\r\n"
							"Content-Transfer-Encoding: base64\r\n"
							"Content-Disposition: attachment\r\n\r\n";
				if(!checkSend(smtp, content.str().c_str())) {
					fclose(f);
					return;
				}
				content.str(std::string()); // clear stream
				content.clear();

				// send endoded 16KB blocks
				base64_init_encodestate(&state);
				while((read = fread(readBuff, 1, 16384, f)) > 0) {
					baseLen = base64_encode_block(readBuff, read, baseBuff, &state);
					if(baseLen > 0) {
						baseBuff[baseLen] = 0;
						if(!checkSend(smtp, baseBuff)) {
							fclose(f);
							return;
						}
					}
				}
				fclose(f);

				// send ending with CRLF
				baseLen = base64_encode_blockend(baseBuff, &state);
				if(baseLen > 0) {
					baseBuff[baseLen] = '\r';
					baseBuff[baseLen+1] = '\n';
					baseBuff[baseLen+2] = 0;
				} else {
					baseBuff[0] = '\r';
					baseBuff[1] = '\n';
					baseBuff[2] = 0;
				}
				if(!checkSend(smtp, baseBuff))
					return;

			} else { // if(f)
				char errBuff[512];
				smtp->failed = true;		
				smtp->disconnect();
				sprintf_s(errBuff, 511, 
					"SMTP - Could not read file %s", extractFilename(msg.attachments[i]).c_str());
				smtp->fail(smtp, errBuff);
				return;
			}
		}
		if(attachments > 0)
			content << "--boundary_protoMail--\r\n";
		
		content << ".\r\n";
	
		if(checkSend(smtp, content.str().c_str()))
			smtp->state = MAILDATAACK;
	}

	WTW_PTR Smtp::onEvent(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		netLibEventDef* ev = reinterpret_cast<netLibEventDef*>(wPar);
		Smtp* smtp = static_cast<Smtp*>(cbData);

		if(smtp->failed) return 0;
		
		switch(ev->eventID) {
			case WTW_NETLIB_EVENT_DATA: 
				{
					netLibDataDef* data = reinterpret_cast<netLibDataDef*>(lPar);
					char* buff = static_cast<char*>(data->data);
					logProtoConsole(smtp->wtw, smtp->netId, buff, data->dataLen, false);
					smtp->lastTimeOutCheck = GetTickCount();

					switch(smtp->state) {
						case CONNECTED:
							checkReceive(smtp, buff, data->dataLen, ehlo,
								"SMTP - On welcome server returned code different than 2xx:\r\n");
							break;
						case PARAMS:
							if(smtp->useSsl && !smtp->secured)
								checkReceive(smtp, buff, data->dataLen, startTls,
									"SMTP - On EHLO server returned code different than 2xx:\r\n");
							else
								checkReceive(smtp, buff, data->dataLen, auth,
									"SMTP - On EHLO server returned code different than 2xx:\r\n");
							break;
						case TLSINIT:
							checkReceive(smtp, buff, data->dataLen, initTls,
								"SMTP - On STARTTLS server returned code different than 2xx:\r\n");
							break;
						case USERNAME:
							checkReceive(smtp, buff, data->dataLen, username,
								"SMTP - After usename sent server returned code different than 2xx or 3xx:\r\n", true);
							break;
						case PASSWORD:
							checkReceive(smtp, buff, data->dataLen, password,
								"SMTP - After password sent server returned code different than 2xx or 3xx:\r\n", true);
							break;
						case LOGGED:
							checkReceive(smtp, buff, data->dataLen, sendMail,
								"SMTP - Authorization error:\r\n");
							break;
						case MAILACK:
							checkReceive(smtp, buff, data->dataLen, sendRecipent,
								"SMTP - On MAIL FROM server returned code different than 2xx:\r\n");
							break;
						case RCPTACK:
							if(checkReceive(smtp, buff, data->dataLen, NULL,
								"SMTP - On RCPT TO server returned code different than 2xx:\r\n")) {
								if(smtp->recipentToSend < smtp->message.recipents.size())
									sendRecipent(smtp);
								else
									sendData(smtp);
							}
							break;
						case DATAACK:
							checkReceive(smtp, buff, data->dataLen, sendMailData,
								"SMTP - On DATA server returned code different than 2xx or 3xx:\r\n", true);
							break;
						case MAILDATAACK:
							if(checkReceive(smtp, buff, data->dataLen, NULL,
								"SMTP - After sending mail content server returned code different than 2xx:\r\n")) {
								smtp->sent(smtp, smtp->message);
								smtp->disconnect();
							}
							break;
					}
					break;
				}
			case WTW_NETLIB_EVENT_SSL:
				{
					netLibSSLStatusDef* sslRes = reinterpret_cast<netLibSSLStatusDef*>(lPar);
					smtp->lastTimeOutCheck = GetTickCount();

					if(sslRes->sslStatus == 1) {
						smtp->secured = true;
						ehlo(smtp); // if EHLO will succed it will set the state to PARAMS
					} else {
						smtp->failed = true;
						smtp->secured = false;
						
						char errBuff[1024];
						if(sslRes->pMessage)
							sprintf_s(errBuff, 1023, "SMTP - SSL error (%d): %s", sslRes->sslInfo, convertEnc(sslRes->pMessage).c_str());
						else
							sprintf_s(errBuff, 1023, "SMTP - SSL error (%d)", sslRes->sslInfo);
						smtp->fail(smtp, errBuff);
					}
					break;
				}
			case WTW_NETLIB_EVENT_CLOSE: 
				{
					if(smtp->state != NOTCONNECTED) {
						smtp->failed = true;
						smtp->secured = false;						
						smtp->sock->close();
						smtp->state = NOTCONNECTED;
						smtp->fail(smtp, "SMTP - Connection closed by server");
					}
					break;
				}
			case WTW_NETLIB_EVENT_ERROR:
				{
					char errBuff[128];
					if(lPar & 0x80000000)
						sprintf_s(errBuff, 127, "SMTP - Connection error: 0x%X", lPar);
					else
						sprintf_s(errBuff, 127, "SMTP - Connection error:%d", lPar);
					smtp->failed = true;
					smtp->secured = false;					
					smtp->sock->close();
					smtp->state = NOTCONNECTED;
					smtp->fail(smtp, errBuff);
					break;
				}
			}
		return 0;
	}

	void Smtp::disconnect() {
		if(state != NOTCONNECTED) {
			int len = strlen("QUIT\r\n");
			state = NOTCONNECTED;
			sock->send("QUIT\r\n", len);
			logProtoConsole(wtw, netId, "QUIT\r\n", len, true);
			sock->close();
			this->secured = false;
		}
	}

	Smtp::Smtp(WTWFUNCTIONS* wtw, ONSENT sent, ONSENTFAIL fail, int netId) {
		this->wtw = wtw;
		this->sent = sent;
		this->fail = fail;
		this->state = NOTCONNECTED;
		this->failed = false;
		this->secured = false;
		this->netId = netId;

		wchar_t	socketName[128];
		swprintf_s(socketName, 128, L"%s/Socket/SMTP/%d", PROTO_CLASS, netId);
		sock = new NetlibSocket(socketName, onEvent, this);
		if(!sock->isValid()) fail(this, "SMTP - Could not create socket");
	}

	void Smtp::sendMail(const smtpAccount& account, const smtpMail& mail) {
		if(state != NOTCONNECTED) {
			char errStr[256];
			sprintf_s(errStr, 255, "SMTP - One connection in progress (state=%d), cannot start new one", state);
			fail(this, errStr);
			return; // do not start next connection before one ends
		}

		this->mail = account.mail;
		smtpServer = account.smtpServer;
		login = account.login;
		pass = account.password;
		useSsl = account.useSsl;				
		message = mail;
		failed = false;
		recipentToSend = 0;
		lastTimeOutCheck = GetTickCount();

		WTW_PTR res;
		hostPort hp = extractHostPort(smtpServer, 587);

		if((res = sock->connect(convertEnc(hp.host).c_str(), hp.port)) != S_OK) {
			char errBuff[128];
			sock->close();
			sprintf_s(errBuff, 127, "SMTP - Could not connect to SMTP server, err=0x%X", res);			
			state = NOTCONNECTED;
			fail(this, errBuff);
			return;
		}

		state = CONNECTED;
	}
};

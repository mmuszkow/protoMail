#include "stdinc.h"

#include "Pop3.h"
#include "../common/Ping.h"

namespace protoMail {
	// checks if received +OK and if so calls onsucc func
	bool Pop3::checkReceive(Pop3* pop3, const char* buff, int len, FUNC onsucc, const char* errstr) {
		if(len > 0 && buff[0] == '+') {
			if(onsucc) onsucc(pop3);
			return true;
		} else {
			pop3->failed = true;
			pop3->disconnect();
			if(len > 0) {
				std::string errBuff = errstr;
				errBuff += buff;
				pop3->fail(pop3, errBuff);
			} else
				pop3->fail(pop3, "POP3 - Received empty buffer");
			return false;
		}
	}

	// Check if sending formatted string succeded
	bool Pop3::checkSend(Pop3* pop3, const char* fmt, ...) {
		va_list ap;
		
		va_start(ap, fmt);
		int len = _vscprintf(fmt, ap);
		char* tmp = new char[len+1];
		vsprintf_s(tmp, len + 1, fmt, ap);
		va_end(ap);
		len = strlen(tmp);

		if(SUCCEEDED(pop3->sock->send(tmp, len))) {
			logProtoConsole(pop3->wtw, pop3->netId, tmp, len, true);
			delete [] tmp;
			pop3->lastTimeOutCheck = GetTickCount();
			return true;
		} else {
			pop3->failed = true;				
			pop3->disconnect();
			delete [] tmp;
			pop3->fail(pop3, "POP3 - Send failed");
			return false;
		}
	}

	// Retrieves message header
	void Pop3::top(Pop3* pop3) {
		// nothing to retrieve
		if(pop3->toBeDownloaded.size() == 0) {
			pop3->disconnect();
			return;
		}

		// get next mail
		int id = pop3->toBeDownloaded.top();
		pop3->toBeDownloaded.pop();
		pop3->lastResult.clear();

		if(checkSend(pop3, "TOP %d 0\r\n", id))
			pop3->state = TOPACK;
	}

	void Pop3::parseUidls(Pop3* pop3) {
		std::istringstream ss(pop3->lastResult.c_str());
		int id;
		std::string uidl;
		
		stdext::hash_set<std::string>::iterator notFound = pop3->uidls.end();
		stdext::hash_set<std::string>& uidls = pop3->uidls;
		bool firstCheck = pop3->firstCheck;
		// parse uidl info
		int i = 0;
		while(1) {
			ss >> id >> uidl;
			if(!ss.good()) break;

			if(uidls.find(uidl) == notFound) {
				if(!firstCheck) toBeDownloaded.push(id);
				uidls.insert(uidl);
			}
			i++;
		}

		pop3->firstCheck = false;
		pop3->lastResult.clear();
		top(pop3);
	}

	// Main socket events loop
	WTW_PTR Pop3::onEvent(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		netLibEventDef* ev = reinterpret_cast<netLibEventDef*>(wPar);
		Pop3* pop3 = static_cast<Pop3*>(cbData);

		if(pop3->failed) return 0;
		
		switch(ev->eventID) {
			// On connection status returned
			case WTW_NETLIB_EVENT_CONNECT: 
				{
					if(pop3->useSsl && (pop3->sock->initSsl() != S_OK)) {
						pop3->failed = true;
						pop3->disconnect();
						pop3->fail(pop3, "POP3 - Starting SSL failed");
					}
					break;
				}
			// On recv
			case WTW_NETLIB_EVENT_DATA: 
				{
					netLibDataDef* data = reinterpret_cast<netLibDataDef*>(lPar);
					char* buff = static_cast<char*>(data->data);
					logProtoConsole(pop3->wtw, pop3->netId, buff, data->dataLen, false);
					pop3->lastTimeOutCheck = GetTickCount();

					switch(pop3->state) {
						case CONNECTED:
							checkReceive(pop3, buff, data->dataLen, user, "POP3 - Error on welcome:\r\n");
							break;
						case USERACK:
							checkReceive(pop3, buff, data->dataLen, pass, "POP3 - Error on USER:\r\n");
							break;
						case PASSACK:
							checkReceive(pop3, buff, data->dataLen, uidl, "POP3 - Error on PASS:\r\n");
							break;
						case UIDL:
							if(checkReceive(pop3, buff, data->dataLen, NULL, "POP3 - Error on UIDL:\r\n")) {
								std::string part(buff);
								size_t pos = part.find("\r\n"); // check if we received +OK\n or +OK\n and the rest of the message
								if(pos + 2 != part.size()) { // we recived some more
									pop3->lastResult.append(part.substr(pos+2).c_str(), data->dataLen-pos-2);
									if(pop3->lastResult.endsWith("\r\n.\r\n")) 
										pop3->parseUidls(pop3);
									else
										pop3->state = UIDLDATA;
								} else
									pop3->state = UIDLDATA;
							}
							break;
						case UIDLDATA:
							pop3->lastResult.append(buff, data->dataLen);
							if(pop3->lastResult.endsWith("\r\n.\r\n"))
								pop3->parseUidls(pop3);
							break;
						case TOPACK:
							if(checkReceive(pop3, buff, data->dataLen, NULL, "POP3 - Error on TOP:\r\n")) {
								std::string part(buff);
								size_t pos = part.find("\r\n"); // check if we received +OK\n or +OK\n and the rest of the message
								if(pos + 2 != part.size()) { // we recived some more
									pop3->lastResult.append(part.substr(pos+2).c_str(), data->dataLen-pos-2);
									if(pop3->lastResult.endsWith("\r\n.\r\n")) 
										pop3->parseMsg(pop3);
									else
										pop3->state = TOPMSG;
								} else
									pop3->state = TOPMSG;
							}
							break;
						case TOPMSG:
							pop3->lastResult.append(buff, data->dataLen);
							if(pop3->lastResult.endsWith("\r\n.\r\n"))
								pop3->parseMsg(pop3);
							break;
					}
					break;
				}
			// On SSL initiation status returned
			case WTW_NETLIB_EVENT_SSL: 
				{
					netLibSSLStatusDef* sslRes = reinterpret_cast<netLibSSLStatusDef*>(lPar);
					pop3->lastTimeOutCheck = GetTickCount();

					if(sslRes->sslStatus == 1) {
						pop3->secured = true;
					} else {
						pop3->failed = true;
						pop3->secured = false;
						
						char errBuff[1024];
						if(sslRes->pMessage)
							sprintf_s(errBuff, 1023, "POP3 - SSL error (%d): %s", sslRes->sslInfo, convertEnc(sslRes->pMessage).c_str());
						else
							sprintf_s(errBuff, 1023, "POP3 - SSL error (%d)", sslRes->sslInfo);
						pop3->fail(pop3, errBuff);
					}
					break;
				}
			// On socket closed (by server)
			case WTW_NETLIB_EVENT_CLOSE: 
				{
					if(pop3->state != NOTCONNECTED) {
						pop3->failed = true;
						pop3->secured = false;
						pop3->sock->close();
						pop3->state = NOTCONNECTED;
						pop3->fail(pop3, "POP3 - Connection closed by server");
					}
					break;
				}
			// On error
			case WTW_NETLIB_EVENT_ERROR:
				{
					char errBuff[128];
					if(lPar & 0x80000000)
						sprintf_s(errBuff, 127, "POP3 - Connection error: 0x%X", lPar);
					else
						sprintf_s(errBuff, 127, "POP3 - Connection error: %d", lPar);
					pop3->failed = true;
					pop3->secured = false;
					pop3->sock->close();
					pop3->state = NOTCONNECTED;
					pop3->fail(pop3, errBuff);
					break;
				}
			}
		return 0;
	}

	// Disconnects (with sending QUIT before closing socket)
	void Pop3::disconnect() {
		if(state != NOTCONNECTED) {
			if(mails.size() > 0) succ(this);
			int len = strlen("QUIT\r\n");
			state = NOTCONNECTED;
			sock->send("QUIT\r\n", len);
			logProtoConsole(wtw, netId, "QUIT\r\n", len, true);
			sock->close();
			this->secured = false;
		}
	}

	Pop3::Pop3(WTWFUNCTIONS* wtw, ONRECVSUCC succ, ONRECVFAIL fail, int netId) {
		this->wtw = wtw;
		this->succ = succ;
		this->fail = fail;
		this->netId = netId;
		this->firstCheck = true;

		wchar_t	socketName[128];
		swprintf_s(socketName, 127, L"%s/Socket/POP3/%d", PROTO_CLASS, netId);
		sock = new NetlibSocket(socketName, onEvent, this);
		if(!sock->isValid()) fail(this, "POP3 - Could not create socket");

		state = NOTCONNECTED;
	}

	// Retrieves new mails if no other connection is in progress
	void Pop3::retrieveNew(const pop3Account& account) {
		if(state != NOTCONNECTED) {
			__LOG_F(wtw, WTW_LOG_LEVEL_INFO, PROTO_CLASS, 
				L"%s: POP3 - One connection in progress (state=%d), cannot start new one", 
				convertEnc(account.mail).c_str(), state);
			return; // do not start next connection before one ends
		}

		if(!ping("www.google.pl"))
			return;

		pop3Server = account.pop3Server;
		mail = account.mail;
		login = account.login;
		password = account.password;
		useSsl = account.useSsl;
		www = account.www;
		failed = false;
		secured = false;
		lastResult.clear();
		while(!toBeDownloaded.empty())
			toBeDownloaded.pop();
		lastTimeOutCheck = GetTickCount();

		WTW_PTR res;
		hostPort hp = extractHostPort(pop3Server, useSsl ? 995 : 110);

		if((res = sock->connect(convertEnc(hp.host).c_str(), hp.port)) != S_OK) {
			char errBuff[128];
			sock->close();
			sprintf_s(errBuff, 127, "POP3 - Could not connect to server, err=0x%X", res);
			state = NOTCONNECTED;
			fail(this, errBuff);
			return;
		}

		state = CONNECTED;
	}
};

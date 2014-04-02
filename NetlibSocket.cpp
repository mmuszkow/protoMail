#include "stdinc.h"
#include "NetlibSocket.h"
#include "PluginController.h"

namespace protoMail {
	NetlibSocket::NetlibSocket(const wchar_t* socketName, WTWFUNCTION callback, void* cbData, SENDSUCC sentCallback) {
		wtw = PluginController::getInstance().getWTWFUNCTIONS();
		wcscpy_s(this->socketName, 127, socketName);
		this->sentCallback = sentCallback;

		netLibCreateDef cDef;
		cDef.callback = callback;
		cDef.id = this->socketName;
		cDef.cbData = cbData;
		cDef.flags = WTW_NETLIB_FLAG_SSL;
		
		valid = (wtw->fnCall(WTW_NETLIB_CREATE, cDef, 0) == S_OK);
	}

	WTW_PTR NetlibSocket::connect(const wchar_t* host, int port) {
		netLibConnectDef cDef;
		cDef.id = socketName;
		cDef.hostAddr = host;
		cDef.hostPort = port;
		cDef.hostAddrFamily = NL_ADDR_FAMILY_IPV4;
		return wtw->fnCall(WTW_NETLIB_CONNECT, cDef, 0);	
	}

	WTW_PTR NetlibSocket::send(const char* buff, int len) {
		netLibSendDef sDef;
		sDef.id = socketName;
		sDef.pBuffer = buff;
		sDef.nBufferLen = len;
		return wtw->fnCall(WTW_NETLIB_SEND, sDef, 0);			
	}
};

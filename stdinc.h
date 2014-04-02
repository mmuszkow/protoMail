#pragma once

#include <Winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <ctime>

#include "plInterface.h"

#include <string>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
#include <set>
#include <hash_set>

#include "../common/StringUtils.h"
using namespace strUtils;
#include "../common/Base64.h"
#include "../common/Settings.h"
#include "../common/AppendString.h"
#include "SettingsConsts.h"

namespace protoMail {
	static const wchar_t PROTO_CLASS[] = L"MAIL";
	static const wchar_t PROTO_NAME[] = L"Mail";

	static INT_PTR bkBrush = reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));

	static bool logProtoConsole(WTWFUNCTIONS* wtw, int netId, const char* data, int len, bool outgoing) {
		wtwProtocolEvent ev;
		ev.event = outgoing ? WTW_PEV_RAW_DATA_SEND : WTW_PEV_RAW_DATA_RECV; 
		ev.netClass = PROTO_CLASS;
		ev.netId = netId;

		wtwRawDataDef rd;
		rd.pData = data;
		rd.pDataLen = len;
		rd.flags = WTW_RAW_FLAG_TEXT | WTW_RAW_FLAG_UTF;

		ev.type = WTW_PEV_TYPE_BEFORE; 
		if(wtw->fnCall(WTW_PF_CALL_HOOKS, ev, rd) == 0) {
			ev.type = WTW_PEV_TYPE_AFTER; 
			wtw->fnCall(WTW_PF_CALL_HOOKS, ev, rd);
			return true;
		}
		return false;
	}
};

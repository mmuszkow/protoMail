#pragma once

#include "stdinc.h"

namespace protoMail {
	class NetlibSocket {
	public:
		typedef void (*SENDSUCC)(const char* buff, int len);
	private:
		wchar_t			socketName[128];
		bool			valid;
		WTWFUNCTIONS*	wtw;
		SENDSUCC		sentCallback;
	public:
		/// Creates socket
		NetlibSocket(const wchar_t* socketName, WTWFUNCTION callback, void* cbData, SENDSUCC sentCallback = NULL);
		/// Destroys socket
		~NetlibSocket() {
			wtw->fnCall(WTW_NETLIB_DESTROY, reinterpret_cast<WTW_PARAM>(socketName), 0);
		}
		/// Is socket created
		inline bool isValid() const {
			return valid;
		}
		/// Connect to host (IPv4)
		WTW_PTR connect(const wchar_t* host, int port);
		/// Send data
		WTW_PTR send(const char* buff, int len);
		/// Init SSL
		inline WTW_PTR initSsl() {
			return wtw->fnCall(WTW_NETLIB_INITSSL, reinterpret_cast<WTW_PARAM>(socketName), NL_SSL_MECH_AUTO);
		}
		/// Disconnect socket
		inline WTW_PTR close() {						
			return wtw->fnCall(WTW_NETLIB_CLOSE, reinterpret_cast<WTW_PARAM>(socketName), 0);
		}		
	};
};

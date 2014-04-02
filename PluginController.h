#pragma once

#include "stdinc.h"
#include "Smtp.h"
#include "SendWindow.h"

namespace protoMail {
	class Protocol;
	
	/** Singleton */
	class PluginController
	{
		// basic
		WTWFUNCTIONS*	wtw;
		HINSTANCE		hInst;

		Protocol*		protocol;
		HANDLE			menuRebuildHook;
		
		HBITMAP			cancelBitmap;
		
		PluginController() : wtw(NULL), hInst(NULL),
			menuRebuildHook(NULL), protocol(NULL), cancelBitmap(NULL) {}
		PluginController(const PluginController&);
		
		static WTW_PTR onSendMailClick(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr);
		static WTW_PTR onMenuRebuild(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr);
		static WTW_PTR onTimeoutTimer(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr);
	public:
		static PluginController& getInstance() {
			static PluginController instance;
			return instance;
		}

		int onLoad(WTWFUNCTIONS *fn);
		int onUnload();

		inline const WTWPLUGINFO* getPlugInfo() {
			static WTWPLUGINFO _plugInfo = {
				sizeof(WTWPLUGINFO),						// struct size
				L"Mail Protocol",							// plugin name
				L"POP3/SMTP with SSL/TLS",					// plugin description
				L"© 2012-2014 Maciej Muszkowski",			// copyright
				L"Maciej Muszkowski",						// author
				L"maciek.muszkowski@gmail.com",				// authors contact
				L"http://www.alset.pl/Maciek",				// authors webpage
				L"",										// url to xml with autoupdate data
				PLUGIN_API_VERSION,							// api version
				MAKE_QWORD(1, 0, 8, 0),						// plugin version
				WTW_CLASS_PROTOCOL,							// plugin class
				NULL,										// function called after "O wtyczce..." pressed
				L"{31558f48-0fa4-483d-99bd-11e9341088c5}",	// guid
				NULL,										// dependencies (list of guids)
				0, 0, 0, 0									// reserved
			};
			return &_plugInfo;
		}

		inline void setDllHINSTANCE(const HINSTANCE h) {
			hInst = h;
		}

		inline HINSTANCE getDllHINSTANCE() const {
			return hInst;
		}

		inline WTWFUNCTIONS* getWTWFUNCTIONS() const {
			return wtw;
		}

		inline Protocol* getProtocol() const {
			return protocol;
		}

		inline HBITMAP getCancelBitmap() const {
			return cancelBitmap;
		}
	};
}; // namespace protoMail

#pragma once

#include "stdinc.h"
#include "PluginController.h"
#include "Account.h"
#include "wtwInputWindows.h"

namespace protoMail {
	class Protocol {		
		HANDLE					hPfSend;

		// Account manager callback, for more look at wtwAccountManager.h
		static WTW_PTR acmCallback(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);
	public:
		// public but add and del only with addAccount delAccount!
		std::vector<Account*> accounts;
		int addAccount(const std::wstring& name);
		void delAccount(int id);

		Protocol();
		~Protocol();

		inline void checkTimeout() {
			unsigned int i, len = accounts.size();
			for(i=0; i<len; i++)
				accounts[i]->checkTimeout();
		}

		// Can return NULL
		inline Account* getPrimaryAccount() {
			if(accounts.size() == 0) return NULL;
			return accounts[0];
		}
	};
};

#include "stdinc.h"
#include "Protocol.h"

namespace protoMail {
	// Account manager callback, for more look at wtwAccountManager.h
	WTW_PTR Protocol::acmCallback(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		wtwACMEvent* ev = reinterpret_cast<wtwACMEvent*>(wPar);
		Protocol* proto = static_cast<Protocol*>(cbData);
		switch(ev->event) {
			// Add all our accounts to list
			case ACM_EVENT_LIST:
				{
					WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
					unsigned int len = proto->accounts.size();
					for(unsigned int i=0; i<len; i++) {
						Account* account = proto->accounts[i];
						// Add our account to accounts list
						wtwACMAccount acmAccount;
						wchar_t accountName[512];
						bool mainAcc = (account->getProfileName().size() == 0);
						swprintf_s(accountName, 511, L"%s [%s]", PROTO_NAME, convertEnc(account->getLogin()).c_str());
						acmAccount.accountName = accountName;
						if(mainAcc) acmAccount.accountInfo = L"Konto główne";
						acmAccount.iconId = WTW_GRAPH_ID_MAIL;
						acmAccount.netClass = PROTO_CLASS;
						acmAccount.netId = account->getId();
						if(!mainAcc) acmAccount.flags = ACM_FLAG_ALLOW_DELETE;
						if(wtw->fnCall(WTW_ACM_ACCOUNT_ADD, acmAccount, 0) != TRUE)
							__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Adding account %s/%d to accounts list failed", PROTO_CLASS, account->getId());
					}
					break;
				}
			case ACM_EVENT_ADD:
				{
					wchar_t	name[33];
					wtwShowMessageWindow wnd;
					wnd.windowType = SMB_TYPE_SIMPLE_INPUT;
					wnd.pRetBuffer = name;
					wnd.pRetBufferSize = 33;
					wnd.windowCaption = PROTO_NAME;
					wnd.windowTitle = L"Nowy profil";
					wnd.windowMessage = L"Podaj nazwę dla nowego profilu, maksymalnie 32 znaki:";

					WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
					if(wtw->fnCall(WTW_SHOW_MESSAGE_BOX, wnd, 0) == SMB_RESULT_OK)
						proto->addAccount(name);
					break;
				}
			case ACM_EVENT_REMOVE:
				{
					wtwACMAccount* account = reinterpret_cast<wtwACMAccount*>(lPar);

					// not our protocol
					if(wcscmp(account->netClass, PROTO_CLASS) != 0) break;

					proto->delAccount(account->netId);
					break;
				}
		}
		return 0;
	}

	int Protocol::addAccount(const std::wstring& name) {
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();			

		// Protocol definition
		wtwProtocolDef pDef;
		pDef.protoCaps1 = WTW_PROTO_CAPS1_MESSAGE;
		pDef.protoCaps2 = WTW_PROTO_CAPS2_UTF;
		pDef.protoDescr = L"Mail Protocol";
		pDef.netClass = PROTO_CLASS;
		pDef.protoName = PROTO_NAME;
		pDef.netGUID = plug.getPlugInfo()->pluginGUID;
		pDef.flags = /*WTW_PROTO_FLAG_PSEUDO |*/ WTW_PROTO_FLAG_NO_MENU | WTW_PROTO_FLAG_NO_PUBDIR;
		pDef.protoIconId = WTW_GRAPH_ID_MAIL;
		if(wtw->fnCall(WTW_PROTO_FUNC_ADD, pDef, 0) != S_OK) {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Adding account failed");
			return -1;
		}

		Account* account = new Account(pDef.netId, name);
		accounts.push_back(account);			
		return account->getId();
	}

	void Protocol::delAccount(int id) {
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();
		int pos = -1;

		// find account with that id
		unsigned int len = accounts.size();
		for(unsigned int i=0; i<len; i++) {
			if(accounts[i]->getId() == id) {
				pos = i;
				break;
			}
		}

		// not found
		if(pos == -1) {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Removing account failed, cannot find account with id = %d", id);
			return;
		}

		Account* account = accounts[pos];
		std::wstring profile;
		// cannot remove main account
		if((profile = account->getProfileName()) == L"") {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Removing account failed, cannot remove main account");
			return;
		}

		// remove from accounts list
		accounts.erase(accounts.begin() + pos);
		delete account;

		// delete config
		wchar_t	dirBuff[MAX_PATH+1];
		wchar_t	filePath[MAX_PATH+1];
		wtwDirectoryInfo dir;
		dir.dirType = WTW_DIRECTORY_PROFILE;
		dir.flags = WTW_DIRECTORY_FLAG_FULLPATH;
		dir.bi.bufferSize = MAX_PATH;
		dir.bi.pBuffer = dirBuff;
		if(wtw->fnCall(WTW_GET_DIRECTORY_LOCATION, dir, 0) == S_OK) {
			swprintf_s(filePath, MAX_PATH, L"%sprotoMail_%s.config", dirBuff, profile.c_str());
			if(DeleteFileW(filePath) != TRUE)
				__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, 
				L"Removing config file for account failed, remove it manually (protoMail-%s.config)", 
				profile.c_str());
		} else
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, 
			L"Removing config file for account failed, remove it manually (protoMail-%s.config)", 
			profile.c_str());
	}

	Protocol::Protocol() {  
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();						

		// Account Manager interface
		wtwACMInterface iAcm;
		iAcm.netClass = PROTO_CLASS;
		//iAcm.netName = PROTO_NAME;  // protoGG daje tutaj NULL
		iAcm.eventCallback = acmCallback;
		iAcm.cbData = this;
		if(wtw->fnCall(WTW_ACM_INTERFACE_ADD, iAcm, 0) != S_OK) {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Adding ACM interface failed");
			return;
		}

		// Add primary account
		addAccount(L"");

		// Other protocol plugs determines accounts by their config name
		// for example: protoGG.config, protoGG_mmm.config will create main account and account mmm
		wchar_t	buff[MAX_PATH+1];
		wtwDirectoryInfo dir;
		dir.dirType = WTW_DIRECTORY_PROFILE;
		dir.flags = WTW_DIRECTORY_FLAG_FULLPATH;
		dir.bi.bufferSize = MAX_PATH;
		dir.bi.pBuffer = buff;
		if(wtw->fnCall(WTW_GET_DIRECTORY_LOCATION, dir, 0) == S_OK) {
			WIN32_FIND_DATA wfd;
			wchar_t parent[MAX_PATH+10];
			swprintf_s(parent, MAX_PATH+9, L"%s*.config", buff);

			HANDLE hFind = FindFirstFile(parent, &wfd);					
			if(hFind != INVALID_HANDLE_VALUE) {
				if(	wcscmp(wfd.cFileName, L"protoMail.config") != 0 && 
					wcsstr(wfd.cFileName, L"protoMail_") != NULL) {
						wchar_t	name[33];
						wcscpy_s(name, 32, &wfd.cFileName[9]);
						int len = wcslen(name);
						if(len > 7) {
							name[wcslen(name)-7] = 0;
							addAccount(name);
						}
					}
				while(FindNextFile(hFind, &wfd) == TRUE) {
					if(	wcscmp(wfd.cFileName, L"protoMail.config") != 0 && 
						wcsstr(wfd.cFileName, L"protoMail_") != NULL) {
						wchar_t	name[33];
						wcscpy_s(name, 32, &wfd.cFileName[10]);
						int len = wcslen(name);
						if(len > 7) {
							name[wcslen(name)-7] = 0;
							addAccount(name);
						}
					}
				}
				FindClose(hFind);
			}
		}
	}

	Protocol::~Protocol() {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
		wtw->fnCall(WTW_ACM_INTERFACE_REMOVE, reinterpret_cast<WTW_PARAM>(PROTO_CLASS), 0);

		unsigned int len = accounts.size();
		for(unsigned int i=0; i<len; i++)
			delete accounts[i];
	}
};

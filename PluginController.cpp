#include "stdinc.h"
#include "PluginController.h"
#include "SettingsPage.h"
#include "SendWindow.h"
#include "Protocol.h"
#include "Smtp.h"

/* 
odszarzyc usuwanie i w ogole je zrobic
*/

namespace protoMail
{
	int PluginController::onLoad(WTWFUNCTIONS *fn)
	{
		// to detect memory leaks
#ifdef _DEBUG
		_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

		wtw = fn; 

		cancelBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_DEL_ATTACH));
					
		// adding item to contact menu Send
		wtwMenuItemDef menuDef;
		menuDef.menuID = WTW_MENU_ID_CONTACT_SEND;
		menuDef.callback = onSendMailClick;
		menuDef.itemId = L"MAIL/fastSend";
		menuDef.menuCaption = L"Wyślij e-mail...";
		menuDef.iconId = WTW_GRAPH_ID_MAIL;
		wtw->fnCall(WTW_MENU_ITEM_ADD, menuDef, NULL); // dunno what it shall return ...			
		menuRebuildHook = wtw->evHook(
			WTW_EVENT_MENU_REBUILD, onMenuRebuild, NULL);
					
		protocol = new Protocol();

		wtwTimerDef timerDef;
		timerDef.id = L"protoMail/timeoutTimer";
		timerDef.sleepTime = 10000; // 10 sec
		timerDef.callback = onTimeoutTimer;
		if(wtw->fnCall(WTW_TIMER_CREATE, timerDef, NULL) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, 
			L"Cannot start timeout timer");

		return 0;
	}

	int PluginController::onUnload()
	{
		// stop timeout timer
		wchar_t	timerName[] = L"protoMail/timeoutTimer";
		wtw->fnCall(WTW_TIMER_DESTROY, reinterpret_cast<WTW_PARAM>(timerName), 0);

		// remove options page
		wtw->fnCall(WTW_OPTION_PAGE_REMOVE, reinterpret_cast<WTW_PARAM>(hInst), reinterpret_cast<WTW_PARAM>(L"protoMail"));		
		
		// remove menu item
		wtwMenuItemDef menuDef;
		menuDef.menuID = WTW_MENU_ID_CONTACT_SEND;
		menuDef.itemId = L"MAIL/fastSend";
		wtw->fnCall(WTW_MENU_ITEM_DELETE, menuDef, NULL);
		
		// unhook and delete what left
		if(menuRebuildHook) wtw->evUnhook(menuRebuildHook);
		if(protocol) delete protocol;
		if(cancelBitmap) DeleteObject(cancelBitmap);

		return 0;
	}
	
	WTW_PTR PluginController::onSendMailClick(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr) {
		wtwMenuPopupInfo* menuPopupInfo = reinterpret_cast<wtwMenuPopupInfo*>(lParam);
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();
		
		int i;
		std::wstringstream name;
		std::vector<std::string> mails;
		std::vector<std::wstring> mailsW;
		
		// get mails for selected contacts
		for(i=0; i<menuPopupInfo->iContacts; i++) {
			wchar_t mail[1024];
			wtwExtData extData;
			extData.contactData = menuPopupInfo->pContacts[i];
			extData.fieldName = WTW_EXTCTC_FIELD_NAME_MAIL;
			extData.retValue = mail;
			extData.fieldBufferLen = 1023;
			wtw->fnCall(WTW_EXTCTD_GET_FIELD, extData, NULL); // dunno what it shall return...
			
			// if has mail
			if(wcslen(mail) > 0) {
				mailsW.push_back(mail);
				mails.push_back(convertEnc(mail));

				WTW_PTR itemHandle = NULL;
				WTW_PTR ret = wtw->fnCall(WTW_CTL_CONTACT_FIND_EX, 
					menuPopupInfo->pContacts[i], 
					reinterpret_cast<WTW_PARAM>(&itemHandle));
				
				if (ret == S_OK && itemHandle) {
					wtwContactListItem cntListItem;
					if (wtw->fnCall(WTW_CTL_CONTACT_GET, itemHandle, cntListItem) == S_OK) {
						if(wcslen(cntListItem.itemText) > 0) {
							if(i == 0)
								name << cntListItem.itemText;
							else
								name << L", " << cntListItem.itemText;
						}
					} else
						__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Could not get contact info via WTW_CTL_GET_CONTACT");
				} else
					__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Could not find contact via WTW_CTL_FND_CONTACT");
			}
		}
	
		unsigned int len = mailsW.size();
		std::wstringstream nameAndMail;
		nameAndMail << name.str() << L" <";
		for(unsigned int j=0; j<len; j++) {
			if(j == 0)
				nameAndMail << mailsW[j];
			else
				nameAndMail << L", " << mailsW[j];
		}
		nameAndMail << L">";

		// create send message window and send the message
		SendWindowResult* res = 
			reinterpret_cast<SendWindowResult*>(DialogBoxParamW(
			plug.getDllHINSTANCE(), MAKEINTRESOURCEW(IDD_SENDWND), 
			0, sendWndProc, reinterpret_cast<LPARAM>(nameAndMail.str().c_str())));
		if(res) {
			if(res->from >= 0 && res->from < plug.getProtocol()->accounts.size())
				plug.getProtocol()->accounts[res->from]->sendMessage(
				convertEnc(name.str()), mails , res->subject, res->msg, res->attachments);
			delete res;
		}
		
		return 0;
	}

	WTW_PTR PluginController::onMenuRebuild(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr) {
		wtwMenuCallbackEvent * event = reinterpret_cast<wtwMenuCallbackEvent*>(wParam);
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();

		int i;
		bool hasMail = false;
		for(i=0; i<event->pInfo->iContacts; i++) {
			// Get contact mail
			wchar_t mail[1024];
			wtwExtData extData;
			extData.contactData = event->pInfo->pContacts[0];
			extData.fieldName = WTW_EXTCTC_FIELD_NAME_MAIL;
			extData.retValue = mail;
			extData.fieldBufferLen = 1023;
			wtw->fnCall(WTW_EXTCTD_GET_FIELD, extData, NULL); // dunno what it shall return...
			if(wcslen(mail) > 0) {
				hasMail = true;
				break;
			}
		}

		// If contact HAS mail show Send mail option in contact menu Send
		if(hasMail) event->slInt.add(event->itemsToShow, L"MAIL/fastSend");
		return 0;
	}

	WTW_PTR PluginController::onTimeoutTimer(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr) {
		wtwTimerEvent* ev = reinterpret_cast<wtwTimerEvent*>(wParam);
		if(ev->event == WTW_TIMER_EVENT_TICK)
			PluginController::getInstance().getProtocol()->checkTimeout();
		return 0;
	}
};

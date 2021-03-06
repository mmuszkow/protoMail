#include "stdinc.h"
#include "Account.h"
#include "PluginController.h"
#include "SettingsPage.h"
#include "utlNews.h"

namespace protoMail {
	Account::Account(int netId, const std::wstring& name) {
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = plug.getWTWFUNCTIONS();

		// basic init
		this->netId = netId;
		this->name = name;
		smtp = new Smtp(wtw, onMailSent, onSendFail, netId);
		pop3 = new Pop3(wtw, onMailRecv, onRecvFail, netId);
		sett = new wtwUtils::Settings(wtw, plug.getDllHINSTANCE(), name.c_str());
		settPage = NULL;

		// Protocol functions
		hPfSend = wtwInstProtoFunc(wtw, PROTO_CLASS, netId, WTW_PF_MESSAGE_SEND, onPfSend, this);
		if(!hPfSend) {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Registering WTW_PF_MESSAGE_SEND failed");
			return;
		}

		// Options page			
		wchar_t id[128];
		swprintf_s(id, 127, L"%s/Options/%d", PROTO_CLASS, netId);

		wtwOptionPageDef pg;
		pg.id = id;
		pg.parentId = WTW_OPTIONS_GROUP_NETWORK;
		if(name.size() == 0)
			pg.caption = PROTO_NAME;
		else {
			wchar_t caption[128];
			swprintf_s(caption, 127, L"%s - %s", PROTO_NAME, name.c_str());
			pg.caption = caption;
		}
		pg.iconId = WTW_GRAPH_ID_MAIL;
		pg.callback = settingsCallback; 
		pg.cbData = this;
		if(wtw->fnCall(WTW_OPTION_PAGE_ADD, reinterpret_cast<WTW_PARAM>(plug.getDllHINSTANCE()), pg) != S_OK) {
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Adding options page for %s/%d failed", PROTO_CLASS, netId);
			return;
		}

		recreateTimer(sett->getInt(config::INTERVAL, 1), false);
		updateAccountInfo();
	}

	void Account::recreateTimer(int minInterval, bool destroyOld) {
		wchar_t	timerName[64];
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
		swprintf_s(timerName, 63, L"%s/%d/mailRecvTimer", PROTO_CLASS, netId);

		if(destroyOld)
			wtw->fnCall(WTW_TIMER_DESTROY, reinterpret_cast<WTW_PARAM>(timerName), 0);

		wtwTimerDef timerDef;
		timerDef.id = timerName;
		timerDef.sleepTime = minInterval * 60000;
		timerDef.cbData = this;
		timerDef.callback = onTimer;

		if(wtw->fnCall(WTW_TIMER_CREATE, timerDef, NULL) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Cannot start new mail check timer for %s/%d", PROTO_CLASS, netId);
	}

	void Account::sendMessage(const std::string& recipentName, const std::vector<std::string>& recipents,
		const std::string& subject, const std::string& message, const std::vector<Attachment>& attachments) {
		smtpAccount	account;
		smtpMail	mail;

		sett->read();
		account.mail = sett->getStr(config::MAIL);
		account.smtpServer = sett->getStr(config::SMTP_SERVER);
		account.login = sett->getStr(config::LOGIN);
		account.password = sett->getStr(config::PASS);
		account.useSsl = (sett->getInt(config::SSL, 1) == 1);

		if(account.smtpServer == "" || account.mail == "" || account.login == "" || account.password == "")
			return; // one or more values not set


		mail.recipentName = recipentName;
		mail.recipents = recipents;
		mail.subject = subject;
		mail.message = message;
		for(int i=0; i<attachments.size(); i++)
			mail.attachments.push_back(attachments[i].fileName);

		smtp->sendMail(account, mail);
	}

	void Account::retieveNewMessages() {
		Pop3::pop3Account account;
		
		sett->read();
		account.pop3Server = sett->getStr(config::POP3_SERVER);		
		account.mail = sett->getStr(config::MAIL);		
		account.login = sett->getStr(config::LOGIN);
		account.password = sett->getStr(config::PASS);
		account.useSsl = (sett->getInt(config::SSL, 1) == 1);
		account.www = sett->getStr(config::HTTP);

		if(account.pop3Server == "" || account.mail == "" || account.login == "" || account.password == "")
			return; // one or more values not set

		pop3->retrieveNew(account);
	}

	std::string Account::getLogin() const {
		if(smtp->getMail() == "") {
			sett->read();
			return sett->getStr(config::MAIL);
		}
		return smtp->getMail();
	}

	Account::~Account() {
		PluginController& plug = PluginController::getInstance();
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();

		// stop timer
		wchar_t	timerName[64];
		swprintf_s(timerName, 63, L"%s/%d/mailRecvTimer", PROTO_CLASS, netId);
		wtw->fnCall(WTW_TIMER_DESTROY, reinterpret_cast<WTW_PARAM>(timerName), 0);

		// remove options page
		wchar_t id[128];
		swprintf_s(id, 127, L"%s/Options/%d", PROTO_CLASS, netId);
		wtw->fnCall(WTW_OPTION_PAGE_REMOVE, reinterpret_cast<WTW_PARAM>(plug.getDllHINSTANCE()), reinterpret_cast<WTW_PARAM>(id));

		// destroy protocol functions
		if(hPfSend) wtw->fnDestroy(hPfSend);

		// remove from WTW
		if(wtw->fnCall(WTW_PROTO_FUNC_DEL, reinterpret_cast<WTW_PARAM>(PROTO_CLASS), netId) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"WTW_PROTO_FUNC_DEL failed");

		// free mem
		if(smtp) delete smtp;
		if(settPage) delete settPage;			
		if(sett) delete sett;
		if(pop3) delete pop3;
	}

	// Settings page callbage
	WTW_PTR Account::settingsCallback(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr) {
		wtwOptionPageShowInfo* info = reinterpret_cast<wtwOptionPageShowInfo*>(wParam);
		Account* account = static_cast<Account*>(ptr);

		wcscpy_s(info->windowCaption, 255, PROTO_NAME);
		wcscpy_s(info->windowDescrip, 511, L"Wysyłanie/odbieranie poczty");

		if (!account->settPage)
			account->settPage = new SettingsPage(info->handle, PluginController::getInstance().getDllHINSTANCE(), account);

		switch (info->action)
		{
		case WTW_OPTIONS_PAGE_ACTION_SHOW:
			account->settPage->show();
			account->settPage->move(info->x, info->y, info->cx, info->cy);
			return 0;
		case WTW_OPTIONS_PAGE_ACTION_MOVE:
			account->settPage->move(info->x, info->y, info->cx, info->cy);
			return 0;
		case WTW_OPTIONS_PAGE_ACTION_HIDE:
			account->settPage->hide();
			return 0;
		case WTW_OPTIONS_PAGE_ACTION_CANCEL:
			account->settPage->cancel();
			delete account->settPage;
			account->settPage = NULL;
			return 0;
		case WTW_OPTIONS_PAGE_ACTION_APPLY:
			account->settPage->apply();
			account->updateAccountInfo();
			return 0;
		case WTW_OPTIONS_PAGE_ACTION_OK:
			account->settPage->apply();
			delete account->settPage;
			account->settPage = NULL;
			return 0;
		}

		return 0;
	}

	// Protocol function - PF_SEND_MESSAGE
	WTW_PTR Account::onPfSend(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		wtwMessageDef* msg = reinterpret_cast<wtwMessageDef*>(wPar);
		if(!msg || !msg->msgMessage || !msg->contactData.id || !msg->contactData.netClass || !cbData) 
			return E_INVALIDARG;

		// send only mail messages
		if(wcscmp(msg->contactData.netClass, PROTO_CLASS) != 0)
			return 0;

		// check if account with this netId exists
		Account* account = static_cast<Account*>(cbData);
		std::string subject;
		if (msg->extInfoCount > 0) {
			for (unsigned int i = 0; i < msg->extInfoCount; i++) {
				if (wcscmp(msg->extInfo[i].name, L"subject") == 0) {
					const wchar_t* subjectInfo = msg->extInfo[i].value;
					subject = subjectInfo ? convertEnc(subjectInfo) : "";
					break;
				}
			}
		}
		std::string recipent = convertEnc(msg->contactData.id);
		std::string message = convertEnc(msg->msgMessage);
		std::vector<std::string> recipents;
		recipents.push_back(recipent);
		account->sendMessage("", recipents, subject, message, std::vector<Attachment>());
		return 0;
	}

	WTW_PTR Account::onTimer(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		wtwTimerEvent* ev = reinterpret_cast<wtwTimerEvent*>(wPar);
		Account* account = static_cast<Account*>(cbData);

		switch(ev->event)
		{
		case WTW_TIMER_EVENT_CREATED:
		case WTW_TIMER_EVENT_TICK:
			{
				account->retieveNewMessages();
				break;
			}
		}
		return 0;
	}

	// Account info
	void Account::updateAccountInfo() {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();

		protocolLoginInfo pLogInfo;
		pLogInfo.loginID = _wcsdup(convertEnc(getLogin()).c_str());
		pLogInfo.loginName = L"Default";
		pLogInfo.netClass = PROTO_CLASS;
		pLogInfo.netId = netId;
		if(wtw->fnCall(WTW_PF_UPDATE_PROTO_INFO, pLogInfo, 0) != S_OK) {
			delete [] pLogInfo.loginID;
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Updating login info for %s/%d failed", PROTO_CLASS, netId);
		}
		delete [] pLogInfo.loginID;

	}

	// On message sent success
	void Account::onMailSent(Smtp* smtp, const smtpMail& mail) {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();

		// show notification
		wtwTrayNotifyDef tray;
		tray.textMain = _wcsdup(convertEnc(mail.recipentName).c_str());
		tray.textLower = L"Mail wysłany pomyślnie";
		tray.iconId = WTW_GRAPH_ID_MAIL;
		tray.graphType = WTW_TN_GRAPH_TYPE_SKINID;
		if(wtw->fnCall(WTW_SHOW_STANDARD_NOTIFY, reinterpret_cast<WTW_PARAM>(&tray), NULL) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Showing notification failed");
		delete [] tray.textMain;

		// add to archive
		wtwMessageExtInfo subject;
		subject.name = L"subject";
		subject.value = _wcsdup(convertEnc(mail.subject).c_str());

		wtwArchiveEntryMessage entry;
		entry.message.contactData.netClass = PROTO_CLASS;
		entry.message.contactData.netId = smtp->getNetId();
		entry.message.contactData.id = _wcsdup(convertEnc(mail.getRecipentsConcated()).c_str());
		entry.message.extInfo = &subject;
		entry.message.extInfoCount = 1;
		entry.message.msgMessage = _wcsdup(convertEnc(mail.message).c_str());
		entry.message.msgTime = time(NULL);
		entry.message.msgFlags = WTW_MESSAGE_FLAG_OUTGOING|WTW_MESSAGE_FLAG_UTF8_MARK;
		entry.chatID = -1;
		
		if(wtw->fnCall(WTW_ARCH_WRITE_MESSAGE, entry, 0) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Writing message to archive failed");

		delete [] entry.message.contactData.id;
		delete [] subject.value;
		delete [] entry.message.msgMessage;
	}

	void Account::onMailRecv(Pop3* pop3) {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();

		// show notification
		const std::vector<pop3Mail>& mails = pop3->getUnreadMails();
		unsigned int i, s = mails.size();

		std::wostringstream title;
		title << L"Nowa poczta (" << convertEnc(pop3->getMail()) << L")";

		std::wostringstream text;
		text << L"Ilość nieprzeczytanych: " << s << L"\r\n";

		for(i=0; i<s; i++) {
			text << i + 1 << L" - " << convertEnc(mails[i].getSender()) <<
				L": \"" << convertEnc(mails[i].getSubject()) << L"\"";

			if(i != s - 1) text << L"\r\n";
		}

		// tray notify
		wtwTrayNotifyDef tray;
		tray.textMain = _wcsdup(title.str().c_str());
		tray.textLower = _wcsdup(text.str().c_str());
		tray.notifierId = _wcsdup(convertEnc(pop3->getMail()).c_str());
		tray.callback = onNotifyClick;
		tray.cbData = pop3;

		tray.iconId = WTW_GRAPH_ID_MAIL;
		tray.classType = WTN_CLASS_MAILINFO;
		tray.flags = WTW_TN_FLAG_OVERRIDE_TIME|WTW_TN_FLAG_TXT_MULTILINE;
		tray.graphType = WTW_TN_GRAPH_TYPE_SKINID;
		if(wtw->fnCall(WTW_SHOW_STANDARD_NOTIFY, tray, NULL) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Showing notification failed");
		delete [] tray.notifierId ;
		delete [] tray.textMain;
		delete [] tray.textLower;

		// news++ entry
		if(wtw->fnExists(UTL_NEWS_ADD_ENTRY)) {
			utlNewsEntry entry;
			std::wstring accountMail = convertEnc(pop3->getMail());
			entry.sourceName = _wcsdup(accountMail.c_str());

			for(i=0; i<s; i++) {
				std::wstring from = convertEnc(mails[i].getSender());
				std::wstring subject = convertEnc(mails[i].getSubject());

				// replace <> in sender's mail
				unsigned int j, fs = from.size();
				for(j=0; j<fs; j++)
					if(from[j] == L'<' || from[j] == L'>')
						from[j] = L' ';

				// create id
				std::wstringstream ss;
				ss << accountMail << "-" << from << "-" << subject << "-" << mails[i].getTime();
				
				// add info to news++
				entry.itemId = _wcsdup(ss.str().c_str());
				entry.title = _wcsdup(from.c_str());
				entry.text = _wcsdup(subject.c_str());
				entry.iconId = WTW_GRAPH_ID_MAIL;
				if(wtw->fnCall(UTL_NEWS_ADD_ENTRY, entry, NULL) != S_OK)
					__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Showing news++ notification failed");

				delete [] entry.text;
				delete [] entry.title;
				delete [] entry.itemId;
			}

			delete [] entry.sourceName;
		}
	}

	void Account::onSendFail(Smtp* smtp, const std::string& err) {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();
		
		__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"%s: %s", 
			convertEnc(smtp->getMail()).c_str(),
			convertEnc(err).c_str());

		MessageBoxA(0, err.c_str(), smtp->getMail().c_str(), MB_ICONERROR);
	}

	void Account::onRecvFail(Pop3* pop3, const std::string& err) {
		WTWFUNCTIONS* wtw = PluginController::getInstance().getWTWFUNCTIONS();

		__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"%s: %s", 
			convertEnc(pop3->getMail()).c_str(),
			convertEnc(err).c_str());

		wtwTrayNotifyDef tray;
		tray.textMain = _wcsdup(convertEnc(pop3->getMail()).c_str());
		tray.textLower = _wcsdup(convertEnc(err).c_str());
		tray.iconId = WTW_GRAPH_ID_WARNING32;
		tray.graphType = WTW_TN_GRAPH_TYPE_SKINID;
		if(wtw->fnCall(WTW_SHOW_STANDARD_NOTIFY, tray, NULL) != S_OK)
			__LOG_F(wtw, WTW_LOG_LEVEL_ERROR, PROTO_CLASS, L"Showing notification failed");
		delete [] tray.textLower;
		delete [] tray.textMain;
	}

	WTW_PTR Account::onNotifyClick(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData) {
		wtwTrayNotifyEvent* ev = reinterpret_cast<wtwTrayNotifyEvent*>(wPar);
		Pop3* pop3 = reinterpret_cast<Pop3*>(cbData);
		if(ev->event == WTW_TN_EVENT_LCLICKED) {
			std::string www = pop3->getMailboxWww();
			if(www.size() > 0)
				ShellExecuteA(NULL, "open", www.c_str(), NULL, NULL, SW_SHOWNORMAL);
		}
		else if(ev->event == WTW_TN_EVENT_DESTROYED)
			pop3->markAllAsRead();
		return 0;
	}
};

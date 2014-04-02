#pragma once

#include "stdinc.h"
#include "Smtp.h"
#include "Pop3.h"
#include "Attachment.h"

namespace protoMail {
	// forward
	class SettingsPage;

	class Account {
		int				netId; // this account net id
		std::wstring	name; // this account (called also profile) name
		Smtp*			smtp; // SMTP implementation
		Pop3*			pop3; // POP3 implementation
		wtwUtils::Settings*	sett; // settings for this account
		SettingsPage*	settPage; // this account settings page
		HANDLE			hPfSend; // handle of WTW_PF_MESSAGE_SEND function

		// Settings page callbage
		static WTW_PTR settingsCallback(WTW_PARAM wParam, WTW_PARAM lParam, void *ptr);

		// Protocol function - PF_SEND_MESSAGE
		static WTW_PTR onPfSend(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);

		// New mail check timer
		static WTW_PTR onTimer(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);

		// On new mail notify click callback
		static WTW_PTR onNotifyClick(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);
	public:
		Account(int netId, const std::wstring& name);

		void sendMessage(const std::string& recipentName, const std::vector<std::string>& recipents,
			const std::string& subject, const std::string& message, 
			const std::vector<Attachment>& attachments);
		void retieveNewMessages();

		/// Returns name (login)
		std::string getLogin() const;

		/// Returns profile name (used in settings), empty string for main account
		const std::wstring& getProfileName() const {
			return const_cast<const std::wstring&>(name);
		}

		/// Creates check timer with specified interval in minutes
		void recreateTimer(int minInterval, bool destroyOld);

		/// Returns netId
		inline int getId() const {
			return netId;
		}

		inline wtwUtils::Settings* getSettings() const {
			return sett;
		}

		// Update account login
		void updateAccountInfo();

		inline void checkTimeout() {
			smtp->checkTimeout();
			pop3->checkTimeout();
		}

		~Account();

	private:
		// On message sent success
		static void onMailSent(Smtp* smtp, const smtpMail& mail);

		// On message send fail
		static void onSendFail(Smtp* smtp, const std::string& err);

		// On new message received
		static void onMailRecv(Pop3* pop3);

		// On message recv fail
		static void onRecvFail(Pop3* pop3, const std::string& err);
	};
};

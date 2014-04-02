#pragma once

#include "stdinc.h"
#include "NetlibSocket.h"
#include "pop3Mail.h"

namespace protoMail {
	class Pop3 {
	public:
		struct pop3Account {
			std::string	pop3Server;
			std::string	mail;
			std::string	login;
			std::string	password;
			std::string	www;
			bool		useSsl;
		};

		typedef void (*ONRECVSUCC)(Pop3* pop3);
		typedef void (*ONRECVFAIL)(Pop3* pop3, const std::string& error);

	private:
		typedef void (*FUNC)(Pop3* pop3);

		WTWFUNCTIONS*	wtw;
		int				netId;
		NetlibSocket*	sock;
		bool			secured;
		bool			failed;
		
		static const int TIMEOUT_MS = 20000;
		DWORD			lastTimeOutCheck;

		// POP3 connection data
		std::string		pop3Server;
		std::string		mail; // used only as id
		std::string		login;
		std::string		password;
		bool			useSsl;
		std::string		www;

		// succes/failure handlers
		ONRECVSUCC		succ;
		ONRECVFAIL		fail;

		// internal state of the protocol
		enum State {
			NOTCONNECTED, CONNECTED, USERACK, PASSACK, UIDL, UIDLDATA, TOPACK, TOPMSG
		};
		State			state;
	
		bool					firstCheck;
		strUtils::AppendString	lastResult;
		stdext::hash_set<std::string>	uidls;
		std::stack<int>			toBeDownloaded;
		std::vector<pop3Mail>	mails;

		// checks if received +OK and if so calls onsucc func
		static bool checkReceive(Pop3* pop3, const char* buff, int len, FUNC onsucc, const char* errstr);

		// Check if sending formatted string succeded
		static bool checkSend(Pop3* pop3, const char* fmt, ...);

		static void user(Pop3* pop3) {
			if(checkSend(pop3, "USER %s\r\n", pop3->login.c_str()))
				pop3->state = USERACK;
		}

		static void pass(Pop3* pop3) {
			if(checkSend(pop3, "PASS %s\r\n", pop3->password.c_str()))
				pop3->state = PASSACK;
		}

		static void uidl(Pop3* pop3) {
			if(checkSend(pop3, "UIDL\r\n"))
				pop3->state = UIDL;
		}

		// Retrieve message headers
		static void top(Pop3* pop3);

		void parseMsg(Pop3* pop3) {
			pop3Mail receivedMail(pop3->lastResult.c_str());
			pop3->mails.push_back(receivedMail);
			top(pop3);
		}

		// Check which UIDLs are not in received list
		void Pop3::parseUidls(Pop3* pop3);

		// Main socket events loop
		static WTW_PTR onEvent(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);
		
		// Disconnects (with sending QUIT before closing socket)
		void disconnect();
	public:
		Pop3(WTWFUNCTIONS* wtw, ONRECVSUCC succ, ONRECVFAIL fail, int netId);

		// Retrieves new mails if no other connection is in progress
		void retrieveNew(const pop3Account& account);

		// Get unread mails 
		const std::vector<pop3Mail>& getUnreadMails() const {
			return mails;
		}

		// Marks all unread as read
		inline void markAllAsRead() {
			mails.clear();
		}

		inline void checkTimeout() {
			if(state == NOTCONNECTED)
				return;

			if(GetTickCount() - lastTimeOutCheck < TIMEOUT_MS)
				return;

			// if two times checked and nothing received
			failed = true;
			secured = false;						
			sock->close();
			state = NOTCONNECTED;
		}

		~Pop3() {
			delete sock;
		}

		inline std::string getMail() const {
			return mail;
		}

		inline std::string getMailboxWww() const {
			return www;
		}
	};
};

#pragma once

#include "stdinc.h"
#include "NetlibSocket.h"
#include "rfc822.h"

namespace protoMail {
	struct smtpAccount {
		std::string	smtpServer;
		std::string	mail;
		std::string	login;
		std::string	password;
		bool		useSsl;
	};

	struct smtpMail {
	public:
		std::string	recipentName;
		std::vector<std::string>	recipents;
		std::string	subject;
		std::string	message;
		std::vector<std::string>	attachments;

		std::string getRecipentsConcated() const {
			unsigned int len = recipents.size();
			if(len == 1)
				return recipents[0];

			std::stringstream	ss;			
			for(unsigned int i=0; i<len; i++) {
				if(i == 0)
					ss << recipents[i];
				else
					ss << ", " << recipents[i];
			}
			return ss.str();
		}
	};

	/** SMTP implementation */
	class Smtp {
		typedef void (*ONSENT)(Smtp* smtp, const smtpMail& mail);
		typedef void (*ONSENTFAIL)(Smtp* smtp, const std::string& error);
		typedef void (*FUNC)(Smtp* smtp);

		// MIME determining
		static const int MIME_TYPES_LEN = 39;
		static const char* MIME_TYPES[MIME_TYPES_LEN][2];
		static const char* getMimeTypeFromExt(const std::string& filepath);

		WTWFUNCTIONS* wtw;

		// SMTP connection data
		std::string	mail; // as id
		std::string	smtpServer; 
		std::string	login;
		std::string	pass;
		bool		useSsl;
		smtpMail	message;
		unsigned int	recipentToSend;

		NetlibSocket*	sock;
		int				netId;
		enum State { 
			// before sending mail
			NOTCONNECTED, CONNECTED, PARAMS, USERNAME, PASSWORD, LOGGED, TLSINIT,
			// mail sending
			MAILACK, RCPTACK, DATAACK, MAILDATAACK
		};
		State		state;
		bool		failed;
		bool		secured;

		static const int TIMEOUT_MS = 20000;
		DWORD		lastTimeOutCheck;

		ONSENT		sent;
		ONSENTFAIL	fail;

		// recv and send with status check
		static bool checkReceive(Smtp* smtp, const char* buff, int len, 
			FUNC onsucc, const char* errstr, bool accept3xx = false);
		static bool checkSend(Smtp* smtp, const char* fmt, ...);

		// EHLO
		static void ehlo(Smtp* smtp) {
			if(checkSend(smtp, "EHLO %s\r\n", smtp->smtpServer.c_str()))
				smtp->state = PARAMS;
		}

		// STARTTLS
		static void startTls(Smtp* smtp) {
			if(checkSend(smtp, "STARTTLS\r\n"))
				smtp->state = TLSINIT;
		}

		// netLib Tls init
		static void initTls(Smtp* smtp) {
			if(smtp->sock->initSsl() != S_OK) {
				smtp->failed = true;				
				smtp->disconnect();
				smtp->fail(smtp, "SMTP - Starting TLS failed");
			}
		}

		// AUTH LOGIN
		static void auth(Smtp* smtp) {
			if(checkSend(smtp, "AUTH LOGIN\r\n"))
				smtp->state = USERNAME;
		}

		// username (base64)
		static void username(Smtp* smtp) {
			if(checkSend(smtp, "%s\r\n", base64encode(smtp->login).c_str()))
				smtp->state = PASSWORD;
		}

		// password (base64)
		static void password(Smtp* smtp) {
			if(checkSend(smtp, "%s\r\n", base64encode(smtp->pass).c_str()))
				smtp->state = LOGGED;
		}

		// MAIL FROM
		static void Smtp::sendMail(Smtp* smtp) {
			if(checkSend(smtp, "MAIL FROM: <%s>\r\n", smtp->mail.c_str()))
				smtp->state = MAILACK;
		}

		// RCPT TO
		static void sendRecipent(Smtp* smtp) {			
			if(checkSend(smtp, "RCPT TO: <%s>\r\n", smtp->message.recipents[smtp->recipentToSend++].c_str()))
				smtp->state = RCPTACK;
		}

		// DATA
		static void sendData(Smtp* smtp) {
			if(checkSend(smtp, "DATA\r\n"))
				smtp->state = DATAACK;
		}

		// tells if string is in 7-bit format
		static bool Smtp::is7bit(const std::string& str);

		//static const int SPECIALS_LEN = 16;
		//static char SPECIALS[SPECIALS_LEN];
		// true if string contains some special chars which cannot be send in plain text
		//static bool containsSpecial(const std::string& str);
		/** 
		  * Deals with non ASCII letters, if such occurs converts 
		  * whole string to format =?UTF-8?B?base64(name)?= 
		  */
		static std::string encodeSmtp(const std::string& utf8);		

		// mail content 
		static void sendMailData(Smtp* smtp);

		static WTW_PTR onEvent(WTW_PARAM wPar, WTW_PARAM lPar, void* cbData);

		void disconnect();
	public:
		Smtp(WTWFUNCTIONS* wtw, ONSENT sent, ONSENTFAIL fail, int netId);	
		~Smtp() {
			disconnect();
			delete sock;
		}

		void sendMail(const smtpAccount& account, const smtpMail& mail);

		inline int getNetId() const {
			return netId;
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
			fail(this, "SMTP - Receive timeout");
		}

		inline std::string getMail() const {
			return mail;
		}
	};
};
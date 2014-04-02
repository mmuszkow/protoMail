#pragma once

#include "stdinc.h"
#include "resource.h"
#include "Account.h"

namespace protoMail {
	class Account;

	class SettingsPage {
		Account*	account;
		HWND		hPanel;

		static INT_PTR CALLBACK dlgProc(HWND, UINT msg, WPARAM, LPARAM) {
			switch(msg) {
				case WM_INITDIALOG: return TRUE;
				case WM_CTLCOLORDLG:
				case WM_CTLCOLORBTN:
				case WM_CTLCOLOREDIT:
				case WM_CTLCOLORSTATIC:
					return bkBrush;
				default: return FALSE;
			}
		}
	public:
		SettingsPage(HWND hParent, HINSTANCE hInst, Account* account) {
			wtwUtils::Settings* sett = account->getSettings();
			this->account = account;
			sett->read();

			hPanel = CreateDialogW(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hParent, dlgProc);

			// set initial data
			SetDlgItemText(hPanel, IDC_MAIL, sett->getWStr(config::MAIL).c_str());
			SetDlgItemText(hPanel, IDC_LOGIN, sett->getWStr(config::LOGIN).c_str());
			SetDlgItemText(hPanel, IDC_PASS, sett->getWStr(config::PASS).c_str());
			SetDlgItemText(hPanel, IDC_SMTP, sett->getWStr(config::SMTP_SERVER).c_str());
			SetDlgItemText(hPanel, IDC_POP3, sett->getWStr(config::POP3_SERVER).c_str());
			SetDlgItemText(hPanel, IDC_HTTP, sett->getWStr(config::HTTP).c_str());
			if(sett->getInt(config::SSL, 1) == 1) 
				CheckDlgButton(hPanel, IDC_SSL, BST_CHECKED);
			SetDlgItemInt(hPanel, IDC_INTERVAL, sett->getInt(config::INTERVAL, 1), TRUE);
		}

		inline void move(int x, int y, int cx, int cy) {
			MoveWindow(hPanel, x, y, cx, cy, TRUE);
		}

		inline void show() {
			ShowWindow(hPanel, SW_SHOW);
		}

		inline void hide() {
			ShowWindow(hPanel, SW_HIDE);
		}

		void apply() {
			bool dirty = false; // true if any of the  (except smtp) changed
			bool intervalChanged = false;
			wtwUtils::Settings* sett = account->getSettings();
			sett->read();

			std::wstring newMail = getDlgItemTextWideStr(hPanel, IDC_MAIL);
			if(sett->getWStr(config::MAIL) != newMail) { // the same in other cases, check if value changed
				sett->setStr(config::MAIL, newMail.c_str());
				sett->write();
				account->updateAccountInfo(); // account id is mail, so we must update it after mail change
				dirty = true;
			}

			std::wstring newSmtpServer = getDlgItemTextWideStr(hPanel, IDC_SMTP);
			if(sett->getWStr(config::SMTP_SERVER) != newSmtpServer)
				sett->setStr(config::SMTP_SERVER, newSmtpServer.c_str());

			std::wstring newPop3Server = getDlgItemTextWideStr(hPanel, IDC_POP3);;
			if(sett->getWStr(config::POP3_SERVER) != newPop3Server) {
				sett->setStr(config::POP3_SERVER, newPop3Server.c_str());
				dirty = true;
			}

			std::wstring newLogin = getDlgItemTextWideStr(hPanel, IDC_LOGIN);
			if(sett->getWStr(config::LOGIN) != newLogin) {
				sett->setStr(config::LOGIN, newLogin.c_str());
				dirty = true;
			}


			std::wstring newPass = getDlgItemTextWideStr(hPanel, IDC_PASS);
			if(sett->getWStr(config::PASS) != newPass) {
				sett->setStr(config::PASS, newPass.c_str());
				dirty = true;
			}

			std::wstring newHttp = getDlgItemTextWideStr(hPanel, IDC_HTTP);
			if(sett->getWStr(config::HTTP) != newHttp)
				sett->setStr(config::HTTP, newHttp.c_str());
			
			bool newUseSsl = (SendMessage(GetDlgItem(hPanel, IDC_SSL), BM_GETCHECK, 0, 0) == BST_CHECKED);
			if((sett->getInt(config::SSL, 1) == 1) != newUseSsl) {			
				sett->setInt(config::SSL, newUseSsl);
				dirty = true;
			}

			int newInterval = GetDlgItemInt(hPanel, IDC_INTERVAL, NULL, TRUE);
			if(newInterval <= 0)
				newInterval = 1;

			if(sett->getInt(config::INTERVAL, 1) != newInterval) {
				sett->setInt(config::INTERVAL, newInterval);
				SetDlgItemInt(hPanel, IDC_INTERVAL, newInterval, TRUE);
				intervalChanged = true;
				dirty = true;
			}
			sett->write();
			
			// if any of important values changed check their validity by checking mail
			if(intervalChanged) account->recreateTimer(newInterval, true);
			if(dirty) account->retieveNewMessages(); 
		}

		inline void cancel() {
		}

		~SettingsPage() {
			DestroyWindow(hPanel);
		}
	};
};

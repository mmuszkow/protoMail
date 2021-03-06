#pragma once

#include "stdinc.h"
#include "SendWindow.h"
#include "Protocol.h"
#include "PluginController.h"
#include "Account.h"
#include "MultiOpenDialog.h"
#include "resource.h"
#include "Attachment.h"

namespace protoMail {
	INT_PTR CALLBACK sendWndProc(HWND hDlg, UINT msg, WPARAM wPar, LPARAM lPar) {
		switch(msg) {
			case WM_INITDIALOG: {
				wchar_t* to = reinterpret_cast<wchar_t*>(lPar);
				Protocol* proto = PluginController::getInstance().getProtocol();
				HWND hAccountCombo = GetDlgItem(hDlg, IDC_FROM);
				for(unsigned int i=0; i<proto->accounts.size(); i++) {
					SendMessage(hAccountCombo, CB_ADDSTRING, 0, 
						reinterpret_cast<LPARAM>(convertEnc(proto->accounts[i]->getLogin()).c_str())); 
				}
				SendMessage(hAccountCombo, CB_SETCURSEL, 0, 0);

				SetDlgItemTextW(hDlg, IDC_TO, to);

				return TRUE;
			}
			case WM_SIZE: {
				int w = LOWORD(lPar);
				int h = HIWORD(lPar);
				RECT r;
				POINT p;

				// buttons
				GetWindowRect(GetDlgItem(hDlg, IDOK), &r);
				int bw = r.right - r.left;
				int bh = r.bottom - r.top;
				MoveWindow(GetDlgItem(hDlg, IDOK), w - MARGIN - (bw<<1) - 6, h - bh - MARGIN, bw, bh, TRUE);
				MoveWindow(GetDlgItem(hDlg, IDCANCEL), w - MARGIN - bw, h - bh - MARGIN, bw, bh, TRUE);

				GetWindowRect(GetDlgItem(hDlg, IDC_ATTACH), &r);
				p.x = r.left;
				ScreenToClient(hDlg, &p);
				MoveWindow(GetDlgItem(hDlg, IDC_ATTACH), p.x, h - bh - MARGIN, bw, bh, TRUE);

				GetWindowRect(GetDlgItem(hDlg, IDC_ATTACH_IMG), &r);
				p.x = r.left;
				ScreenToClient(hDlg, &p);
				int iw = r.right - r.left;
				MoveWindow(GetDlgItem(hDlg, IDC_ATTACH_IMG), p.x, h - bh - MARGIN, iw, bh, TRUE);

				GetWindowRect(GetDlgItem(hDlg, IDC_ATTACH_COUNT), &r);
				p.x = r.left;
				ScreenToClient(hDlg, &p);
				MoveWindow(GetDlgItem(hDlg, IDC_ATTACH_COUNT), p.x, h - bh - MARGIN, w - (bw*3) - (MARGIN<<1) - 10 - iw, bh, TRUE);
			
				// message field size
				GetWindowRect(GetDlgItem(hDlg, IDC_SUBJ_LABEL), &r);
				p.x = r.left;
				p.y = r.bottom;
				ScreenToClient(hDlg, &p);
				MoveWindow(GetDlgItem(hDlg, IDC_MSG), p.x, p.y + 2, w - (MARGIN<<1) + 2, h - p.y - 2 - (MARGIN<<1) - bh, TRUE);

				return TRUE;
			}
			case WM_CTLCOLORDLG:
			case WM_CTLCOLORBTN:
			case WM_CTLCOLOREDIT:
			case WM_CTLCOLORSTATIC:
				return bkBrush;
			case WM_GETMINMAXINFO: {
				PMINMAXINFO minMax = reinterpret_cast<PMINMAXINFO>(lPar);
				RECT r;
				POINT p;
				GetWindowRect(GetDlgItem(hDlg, IDC_MIN_HOLDER), &r);
				p.x = r.right;
				p.y = r.bottom;
				ScreenToClient(hDlg, &p);
				minMax->ptMinTrackSize.x = p.x + (MARGIN<<1);
				minMax->ptMinTrackSize.y = p.y + MARGIN;
				return TRUE;
			}
			case WM_DESTROY: {
				HANDLE hAttach = GetProp(hDlg, L"protoMail_Attachments");
				if(hAttach)
					delete reinterpret_cast<std::vector<Attachment>*>(hAttach);
				return TRUE;
			}
			case WM_COMMAND: {
					HANDLE hAttach = GetProp(hDlg, L"protoMail_Attachments");
					std::vector<Attachment>* files = 
						hAttach ? reinterpret_cast<std::vector<Attachment>*>(hAttach) : NULL;

					// deleting files
					if(files && wPar >= IDC_ATTACH + 667 && wPar < IDC_ATTACH + 667 + files->size()) {
						files->erase(files->begin() + (wPar - IDC_ATTACH - 667));
						if(files->size() == 0) {
							ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_IMG), SW_HIDE);
							ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_COUNT), SW_HIDE);
						} else {
							wchar_t buff[32];
							__int64 total = 0;
							for(unsigned int i=0; i<files->size(); i++)
								total += files->at(i).size;
							readable_size(files->size(), total, buff, 32);
							SetDlgItemTextW(hDlg, IDC_ATTACH_COUNT, buff);
							ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_IMG), SW_SHOW);
							ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_COUNT), SW_SHOW);
						}
					}

					switch(wPar) {
						case IDC_ATTACH: {
								POINT pos;
								GetCursorPos(&pos);

								// get popup menu
								PluginController& plug = PluginController::getInstance();
								HINSTANCE hInst = plug.getDllHINSTANCE();
								HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_ATTACH));
								HMENU hPopup = GetSubMenu(hMenu, 0);

								if(files) {
									unsigned int s = files->size();
									if(s > 0) {
										// insert separator
										MENUITEMINFOA mii;
										mii.cbSize = sizeof(MENUITEMINFOA);
										mii.fMask = MIIM_ID|MIIM_FTYPE|MIIM_STATE;
										mii.wID = IDC_ATTACH + 666;
										mii.fType = MFT_SEPARATOR;
										mii.fState = MFS_ENABLED;
										InsertMenuItemA(hPopup, mii.wID, TRUE, &mii);

										// insert file names
										mii.fMask = MIIM_BITMAP|MIIM_STRING|MIIM_FTYPE|MIIM_ID|MIIM_STATE; 
										mii.fType = MFT_STRING;	
										mii.hbmpItem = plug.getCancelBitmap();
										for(unsigned int i=0; i<s; i++) {
											mii.wID = IDC_ATTACH + 667 + i;
											mii.dwTypeData = const_cast<LPSTR>(files->at(i).fileName.c_str()); 
											mii.cch = files->at(i).fileName.size();
											InsertMenuItemA(hPopup, mii.wID, TRUE, &mii); 
										}
									}
								}

								TrackPopupMenu(hPopup, TPM_LEFTALIGN, pos.x, pos.y, 0, hDlg, NULL);

								return TRUE;
							}
						case ID_ATT_ADD: {
								HINSTANCE hInst = PluginController::getInstance().getDllHINSTANCE();
								
								HANDLE hAttach = GetProp(hDlg, L"protoMail_Attachments");
								std::vector<Attachment>* files = NULL;
								if(hAttach)
									files = reinterpret_cast<std::vector<Attachment>*>(hAttach);
								else {
									files = new std::vector<Attachment>();
									SetProp(hDlg, L"protoMail_Attachments", files);
								}

								std::vector<std::string> odRes;
								openDialog(hDlg, hInst, odRes);
								for(unsigned int i=0; i<odRes.size(); i++)
									files->push_back(Attachment(odRes[i]));

								if(files->size() == 0) {
									ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_IMG), SW_HIDE);
									ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_COUNT), SW_HIDE);
								}
								else {
									wchar_t buff[32];
									__int64 total = 0;
									for(unsigned int i=0; i<files->size(); i++)
										total += files->at(i).size;
									readable_size(files->size(), total, buff, 32);
									SetDlgItemTextW(hDlg, IDC_ATTACH_COUNT, buff);
									ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_IMG), SW_SHOW);
									ShowWindow(GetDlgItem(hDlg, IDC_ATTACH_COUNT), SW_SHOW);
								}

								return TRUE;
							}
						case IDOK: {
								SendWindowResult* res = new SendWindowResult();
								res->from = static_cast<int>(
									SendMessage(GetDlgItem(hDlg, IDC_FROM), CB_GETCURSEL, 0, 0));
								res->subject = convertEnc(getDlgItemTextWideStr(hDlg, IDC_SUBJECT));
								res->msg = convertEnc(getDlgItemTextWideStr(hDlg, IDC_MSG));

								HANDLE hAttach = GetProp(hDlg, L"protoMail_Attachments");
								if(hAttach)
									res->attachments = *(reinterpret_cast<std::vector<Attachment>*>(hAttach));

								EndDialog(hDlg, reinterpret_cast<INT_PTR>(res));
								return TRUE;
							}
						case IDCANCEL:
							EndDialog(hDlg, NULL);
							return TRUE;
					}
					return FALSE;
				}
			default: return FALSE;
		}
	}
};

#pragma once

#include "stdinc.h"

bool openDialog(HWND hOwner, HINSTANCE hInst, std::vector<std::string>& result) {
	// initalize open dialog struct
	char path[MAX_PATH+1] = {0};
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(OPENFILENAMEA));
	char dir[MAX_PATH+1];
	GetCurrentDirectoryA(MAX_PATH, dir);
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hOwner;
	ofn.hInstance = hInst;
	ofn.lpstrFilter = "Wszystkie pliki\0*.*\0";
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFile = path;
	ofn.lpstrInitialDir = dir;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	
	// if clicked open not cancel
	if (GetOpenFileNameA(&ofn) == TRUE) {
		// get directory
		std::string dir(path);
		dir = dir.substr(0, ofn.nFileOffset);
		if(dir.size()==0) return false;

		// must end with \ or /
		if(dir[dir.size()-1] != '\\' && dir[dir.size()-1] != '/')
			dir += "\\";

		// extract filenames
		int pos = ofn.nFileOffset;
		while(pos < MAX_PATH + 1) {
			std::string filename(path + pos);
			if(filename.size()==0)
				break;
			else {
				std::string filepath(dir);
				filepath += filename;
				result.push_back(filepath);
			}
			pos += filename.size()+1;
		}

		return true;
	}
	else if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL)
		MessageBoxW(hOwner, L"Zbyt d³uga œcie¿ka lub zbyt wiele wybranych plików", L"Za³¹czniki", MB_OK);

	return false;
}

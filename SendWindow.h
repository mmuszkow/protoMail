#pragma once

#include "stdinc.h"
#include "Attachment.h"

namespace protoMail {
	struct SendWindowResult {
		int			from; // index of an account
		std::string subject;
		std::string msg;
		std::vector<Attachment> attachments;
	};

	static const int MARGIN = 14;

	INT_PTR CALLBACK sendWndProc(HWND hDlg, UINT msg, WPARAM wPar, LPARAM lPar);

	static void readable_size(unsigned int count, __int64 bytes, wchar_t* buff, size_t buff_len) {
		if(bytes > 1073741824)
			swprintf_s(buff, buff_len-1, L"%u [%.2fGB]", count, bytes / 1073741824.0);
		else if (bytes > 1048576)
			swprintf_s(buff, buff_len-1, L"%u [%.2fMB]", count, bytes / 1048576.0);
		else if(bytes > 1024)
			swprintf_s(buff, buff_len-1, L"%u [%.2fKB]", count, bytes / 1024.0);
		else
			swprintf_s(buff, buff_len-1, L"%u [%I64dB]", count, bytes);
	}
};

#pragma once

#include "stdinc.h"

namespace protoMail {
	struct Attachment {
		std::string	fileName;
		__int64		size;

		Attachment(std::string fileName) {
			FILE* f;
			this->fileName = fileName;			
			fopen_s(&f, fileName.c_str(), "rb");
			if(f) {
				_fseeki64(f, 0, SEEK_END);
				this->size = _ftelli64(f);
				fclose(f);
			}
		}
	};
};

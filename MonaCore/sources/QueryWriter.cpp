/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Mona/QueryWriter.h"


using namespace std;


namespace Mona {

void QueryWriter::clear() {
	_isProperty = false;
	_first = true;
	DataWriter::clear();
}

void QueryWriter::writePropertyName(const char* value) {
	writeString(value, strlen(value));
	_isProperty = true;
}

void QueryWriter::writeString(const char* value, UInt32 size) {
	if (uriChars)
		write(String::URI(value, size));
	else
		write(value);
}

UInt64 QueryWriter::writeBytes(const UInt8* data, UInt32 size) {
	write();
	if(_pBuffer)
		Util::ToBase64(data, size, *_pBuffer, true);
	else
		Util::ToBase64(data, size, writer, true);
	return 0;
}

} // namespace Mona

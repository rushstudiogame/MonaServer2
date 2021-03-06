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

#pragma once

#include "Mona/Mona.h"
#include "Mona/TCProtocol.h"
#include "Mona/RTMP/RTMPSession.h"

namespace Mona {

class RTMProtocol : public TCProtocol, public virtual Object {
public:
	RTMProtocol(const char* name, ServerAPI& api, Sessions& sessions, const shared<TLS>& pTLS=nullptr) : TCProtocol(name, api, sessions, pTLS) {

		setNumber("port", pTLS ? 8443 : 1935);
		setNumber("timeout", 60); // 60 seconds

		onConnection = [this](const shared<Socket>& pSocket) {
			// Create session
			this->sessions.create<RTMPSession>(self, pSocket).connect();
		};
	}
	~RTMProtocol() { onConnection = nullptr; }

};


} // namespace Mona

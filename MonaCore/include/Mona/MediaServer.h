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
#include "Mona/MediaSocket.h"
#include "Mona/SRT.h"
#include "Mona/HTTP/HTTPDecoder.h"

namespace Mona {

struct MediaServer : virtual Static {
	enum Type {
		TYPE_TCP = 1, // to match MediaStream::Type
#if defined(SRT_API)
		TYPE_SRT = 2, // to match MediaStream::Type
#endif
		TYPE_HTTP = 4 // to match MediaStream::Type
	};

	struct Reader : MediaStream, virtual Object {
		static unique<MediaServer::Reader> New(MediaServer::Type type, const Path& path, Media::Source& source, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static unique<MediaServer::Reader> New(MediaServer::Type type, const Path& path, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, source, path.extension().c_str(), address, io, pTLS); }

		Reader(MediaServer::Type type, const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Reader() { stop(); }

		const SocketAddress		address;
		IOSocket&				io;
		shared<const Socket>	socket() const { return _pSocket ? _pSocket : nullptr; }

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream server source ", TypeToString(type), "://", address, path, '|', String::Upper(_pReader ? _pReader->format() : "AUTO")); }

		Socket::OnAccept	_onConnnection;
		Socket::OnError		_onError;

		shared<MediaReader>			_pReader;
		shared<Socket>					_pSocket;
		shared<TLS>						_pTLS;
		shared<MediaSocket::Reader>	_pTarget;
		bool						_streaming;
	};

	struct Writer : MediaStream, virtual Object {
		static unique<MediaServer::Writer> New(MediaServer::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static unique<MediaServer::Writer> New(MediaServer::Type type, const Path& path, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, path.extension().c_str(), address, io, pTLS); }

		Writer(MediaServer::Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Writer() { stop(); }

		const SocketAddress		address;
		IOSocket&				io;
		shared<const Socket>	socket() const { return _pSocket ? _pSocket : nullptr; }

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream server target ", TypeToString(type), "://", address, path, '|', String::Upper(_format)); }

		Socket::OnAccept		_onConnnection;
		Socket::OnError		_onError;

		shared<Socket>			_pSocket;
		shared<TLS>				_pTLS;
		const char*						_subMime;
		const char*						_format;
	};
};

} // namespace Mona

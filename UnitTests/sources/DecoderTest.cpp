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

#include "Mona/UnitTest.h"
#include "Mona/UDPSocket.h"
#include "Mona/FileReader.h"
#include "Mona/Util.h"

using namespace Mona;
using namespace std;

namespace DecoderTest {

static ThreadPool _ThreadPool;
static struct MainHandler : Handler {
	MainHandler() : Handler(_signal) {}
	UInt32 join(UInt32 min) {
		UInt32 count(0);
		while ((count += Handler::flush()) < min && _signal.wait(14000));
		return count;
	}

private:
	void flush() {}
	Signal _signal;
} _Handler;

struct Decoded : virtual Object, Packet {
	Decoded(const Packet& packet, const SocketAddress& address) : address(address), Packet(move(packet)) {}
	const SocketAddress address;
};

struct Decoder : virtual Object, Socket::Decoder {
	typedef Event<void(Decoded&)> ON(Decoded);

	Decoder() : count(0) {}
	UInt8	count;

	void decode(Packet& packet, const SocketAddress& address) {
		CHECK(Thread::CurrentId() != Thread::MainId);
		CHECK(!count);
		do {
			if (count++)
				_Handler.queue(onDecoded, Packet(packet, packet.data() + 2, 3), address);  // second time
			else
				_Handler.queue(onDecoded, Packet(packet, packet.data(), 5), address);  // first time
		} while (packet += 5);
	}
private:
	void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
		Packet packet(pBuffer);
		decode(packet, address);
	}
};


ADD_TEST(Manual) {

	Exception ex;
	Decoder decoder;

	UInt8 count(0);
	SocketAddress address(IPAddress::Loopback(), Util::Random<UInt16>());
	decoder.onDecoded = [&](Decoded& decoded) {
		CHECK(Thread::CurrentId()==Thread::MainId);
		CHECK(count < 2);
		CHECK(memcmp(decoded.data(), (count++ ? "msg" : "hello"), decoded.size())==0);
		CHECK(decoded.address == address);
	};
	std::thread([&] {
		Packet packet(EXPAND("hello10msg"));
		decoder.decode(packet, address); }
	).join();
	CHECK(_Handler.join(2)==2 && count == 2 && decoder.count == count);
}


ADD_TEST(Socket) {

	Exception ex;
	IOSocket io(_Handler,_ThreadPool);
	Socket sender(Socket::TYPE_DATAGRAM);

	struct UDPReceiver : UDPSocket {
		UDPReceiver(IOSocket& io) : UDPSocket(io), _count(0),
			_onDecoded([this](Decoded& decoded) {
				CHECK(Thread::CurrentId() == Thread::MainId);
				CHECK(_count < 2);
				CHECK(memcmp(decoded.data(), (_count++ ? "msg" : "hello"), decoded.size()) == 0);
			}) {
		}
		~UDPReceiver() { _onDecoded = nullptr; }
	private:
		Socket::Decoder* newDecoder() {
			Decoder* pDecoder = new Decoder();
			pDecoder->onDecoded = _onDecoded;
			return pDecoder;
		}
		UInt8			_count;
		Decoder::OnDecoded _onDecoded;
	} receiver(io);

	CHECK(receiver.bind(ex, SocketAddress::Wildcard()) && !ex);

	CHECK(sender.sendTo(ex, EXPAND("hello10msg"), SocketAddress(IPAddress::Loopback(), receiver->address().port())) == 10 && !ex)

	CHECK(_Handler.join(3)==3); // 3 with IOSocket::Write of UDPReceiver
}


ADD_TEST(File) {

	struct Decoded : Packet {
		Decoded(const Packet& packet, bool end) : end(end), Packet(move(packet)) {}
		const bool			end;
	};
	struct Decoder : File::Decoder {
		typedef Event<void(Decoded&)> ON(Decoded);
	private:
		UInt32 decode(shared<Buffer>& pBuffer, bool end) {
			CHECK(Thread::CurrentId() != Thread::MainId);
			Packet packet(pBuffer);
			_Handler.queue(onDecoded, packet, end);
			if (end)
				return 0;
			CHECK(packet.size() == 0xFFFF);
			return packet.size();
		}
	};
	struct Reader : FileReader {
		Reader(IOFile& io) :FileReader(io), count(0),
			_onDecoded([this](Decoded& decoded) {
				CHECK(Thread::CurrentId() == Thread::MainId);
				if (!decoded.end)
					CHECK(decoded.size() == 0xFFFF);
				++count;
			}) {
		}
		~Reader() { _onDecoded = nullptr; }
		UInt8	count;

	private:
		File::Decoder* newDecoder() {
			Decoder* pDecoder = new Decoder();
			pDecoder->onDecoded = _onDecoded;
			return pDecoder;
		}
		Decoder::OnDecoded _onDecoded;
	};

	Exception ex;
	IOFile io(_Handler, _ThreadPool);
	Reader reader(io);
	reader.onError = [](const Exception& ex) { FATAL_ERROR(ex); };

	CHECK(reader.open(Path::CurrentApp()));
	reader.read();
	io.join();

	UInt32 min(UInt32(Path::CurrentApp().size() / 0xFFFF));
	CHECK(_Handler.join(min) == reader.count && reader.count >= min);
}

}

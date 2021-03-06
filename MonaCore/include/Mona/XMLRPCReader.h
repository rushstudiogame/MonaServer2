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
#include "Mona/DataReader.h"
#include "Mona/XMLParser.h"
#include <vector>


namespace Mona {

class XMLRPCReader : public DataReader, private XMLParser, public virtual Object {
public:
	XMLRPCReader(const UInt8* data, UInt32 size);

	enum XMLName {
		ARRAY = OTHER + 2,
		PARAMS,
		PARAM,
		VALUE,
		DATA,
		STRUCT,
		MEMBER,
		NAME,
		TYPE,
		UNKNOWN
	};

	void	reset();

	/// \brief Return true if content is valid
	bool	isValid() const { return _validating==0; }


private:

	bool	readOne(UInt8 type, DataWriter& writer);
	UInt8	followingType();

	enum {
		OBJECT =		OTHER,
		OBJECT_TYPED =	OTHER+1,
	};

	UInt8	parse();

	bool					_first;
	UInt8					_nextType;
	const char*				_data;
	std::string				_attribute;
	UInt32					_size;
	std::vector<XMLName>	_xmls;
	UInt32					_countingLevel;

	// not reseted
	UInt8		_validating;
	std::string _method;
	XMLState	_state;
	bool		_isResponse;

	// XML Parser implementation

	bool onStartXMLElement(const char* name, Parameters& attributes);
	bool onInnerXMLElement(const char* name, const char* data, UInt32 size);
	bool onEndXMLElement(const char* name);

};


} // namespace Mona

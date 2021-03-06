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

#include "Service.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {



Service::Service(lua_State* pState, const string& wwwPath, Handler& handler, IOFile& ioFile) :
	_handler(handler), _wwwPath(wwwPath), _reference(LUA_REFNIL), _pParent(NULL), _pState(pState),
	_pWatcher(SET, Path(wwwPath, "*"), FileSystem::MODE_HEAVY), file(wwwPath, "/main.lua"),
	_onUpdate([this](const Path& file, bool firstWatch) {
		if(!file.isFolder() && file.name() != "main.lua")
			return;
		// file path is a concatenation of _wwwPath given to ListFiles in FileWatcher and file name, so we can get relative file name!
		const char* path = (file.isFolder() ? file.c_str() : file.parent().c_str()) + _wwwPath.size();
		Service* pService;
		if (file.exists()) {
			DEBUG("Application ", file, " update");
			pService = &open(path);
			if (file.isFolder())
				pService = NULL; // no children update required here!
			else
				pService->start();
		} else {
			DEBUG("Application ", file, " deletion");
			if (!file.isFolder()) {
				Exception ex;
				pService = get(ex, path);
				if (pService)
					pService->stop();
			} else
				pService = close(path);
		}
		// contamine reload to children application!
		if (pService && !firstWatch) {
			for (auto& it : pService->_services)
				it.second.update();
		}
	}) {
	ioFile.watch(_pWatcher, _onUpdate);
	init();
}

Service::Service(lua_State* pState, const string& wwwPath, Service& parent, const char* name) :
	_handler(parent._handler), _wwwPath(wwwPath), _reference(LUA_REFNIL), _pParent(&parent), _pState(pState),
	name(name), file(parent.file.parent(), name, "/main.lua") {
	String::Assign((string&)path, parent.path,'/',name);
	init();
}

Service::~Service() {
	// clean environnment
	stop();
	// release reference
	luaL_unref(_pState, LUA_REGISTRYINDEX, _reference);
}

Service* Service::get(Exception& ex, const char* path) {
	if (*path == '/')
		++path; // remove first '/'
	if (!*path)
		return (_ex = ex) ? NULL : this;
	const char* subPath = strchr(path, '/');
	string name(path, subPath ? (subPath - path) : strlen(path));
	const auto& it = _services.find(name);
	if (it != _services.end())
		return it->second.get(ex, subPath ? subPath : "");
	ex.set<Ex::Application::Unfound>("Application ", this->path, '/', name, " doesn't exist");
	return NULL;
}
Service& Service::open(const char* path) {
	if (!*path)
		return self;
	const char* subPath = strchr(path, '/');
	if (!subPath)
		return _services.emplace(SET, forward_as_tuple(path), forward_as_tuple(_pState, _wwwPath, self, path)).first->second;
	String::Scoped scoped(subPath);
	return _services.emplace(SET, forward_as_tuple(path), forward_as_tuple(_pState, _wwwPath, self, path)).first->second.open(subPath + 1);
}
Service* Service::close(const char* path) {
	if (*path == '/')
		++path; // remove first '/'
	if (!*path)
		return _services.empty() ? NULL : this;
	const char* subPath = strchr(path, '/');
	const auto& it = _services.find(string(path, subPath ? (subPath - path) : strlen(path)));
	if (it == _services.end())
		return NULL;
	Service* pService = it->second.close(subPath ? subPath : "");
	if(!pService)
		_services.erase(it);
	return pService;
}

void Service::update() {
	if(!_ex)
		start(); // restart!
	for (auto& it : _services)
		it.second.update();
}


void Service::init() {
	_ex.set<Ex::Unavailable>(); // not loaded!
	//// create environment

	// table environment
	lua_newtable(_pState);

	// metatable
	Script::NewMetatable(_pState);

	// index
	lua_pushliteral(_pState, "__index");
	lua_newtable(_pState);

	// set name
	lua_pushliteral(_pState, "name");
	lua_pushlstring(_pState, name.data(), name.size());
	lua_rawset(_pState, -3);

	// set path
	lua_pushliteral(_pState, "path");
	lua_pushlstring(_pState, path.data(), path.size());
	lua_rawset(_pState, -3);

	// set this
	lua_pushliteral(_pState, "this");
	lua_pushvalue(_pState, -5);
	lua_rawset(_pState, -3);

	// metatable of index = parent or global (+ set super)
	Script::NewMetatable(_pState); // metatable
	lua_pushliteral(_pState, "__index");
	if (_pParent)
		lua_rawgeti(_pState, LUA_REGISTRYINDEX, _pParent->reference());
	else
		lua_pushvalue(_pState, LUA_GLOBALSINDEX);
	/// set super
	lua_pushliteral(_pState, "super");
	lua_pushvalue(_pState, -2);
	lua_rawset(_pState, -6);
	lua_rawset(_pState, -3); // set __index
	lua_setmetatable(_pState, -2);

	lua_rawset(_pState, -3); // set __index

	// set metatable
	lua_setmetatable(_pState, -2);

	// record in registry
	_reference = luaL_ref(_pState, LUA_REGISTRYINDEX);
}

void Service::start() {
	stop();

	_ex = nullptr;

	SCRIPT_BEGIN(_pState)
		lua_rawgeti(_pState, LUA_REGISTRYINDEX, _reference);
		int error = luaL_loadfile(_pState, file.c_str());
		if(!error) {
			lua_pushvalue(_pState, -2);
			lua_setfenv(_pState, -2);
			if (lua_pcall(_pState, 0, 0, 0) == 0) {
				SCRIPT_FUNCTION_BEGIN("onStart", _reference)
					SCRIPT_WRITE_DATA(path.data(), path.size())
					SCRIPT_FUNCTION_CALL
				SCRIPT_FUNCTION_END
				INFO("Application www", path, " started")
			} else
				SCRIPT_ERROR(_ex.set<Ex::Application::Invalid>(Script::LastError(_pState)));
		} else if (error==LUA_ERRFILE) // can happen on update when a parent directory is deleted!
			_ex.set<Ex::Application::Unfound>(Script::LastError(_pState));
		else
			SCRIPT_ERROR(_ex.set<Ex::Application::Invalid>(Script::LastError(_pState)));
		lua_pop(_pState, 1); // remove environment
		if(_ex)
			stop();
	SCRIPT_END
}

void Service::stop() {

	if (!_ex) { // loaded!
		SCRIPT_BEGIN(_pState)
			SCRIPT_FUNCTION_BEGIN("onStop", _reference)
				SCRIPT_WRITE_DATA(path.data(), path.size())
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
		INFO("Application www", path, " stopped");

		// update signal, after onStop because will disconnects clients
		_handler.onUnload(self);
		_ex.set<Ex::Unavailable>(); // not loaded!
	}

	// Clear environment (always do, super can assign value on this app even if empty)
	/// clear environment table
	lua_rawgeti(_pState, LUA_REGISTRYINDEX, _reference);
	lua_pushnil(_pState);  // first key 
	while (lua_next(_pState, -2)) {
		// uses 'key' (at index -2) and 'value' (at index -1) 
		// remove the raw!
		lua_pushvalue(_pState, -2); // duplicate key
		lua_pushnil(_pState);
		lua_rawset(_pState, -5);
		lua_pop(_pState, 1);
	}
	/// clear index of metatable (used by few object like Timers, see LUATimer)
	lua_getmetatable(_pState, -1);
	int count = lua_objlen(_pState, -1);
	for (int i = 1; i <= count; ++i) {
		lua_pushnil(_pState);
		lua_rawseti(_pState, -2, i);
	}
	lua_pop(_pState, 2); // remove metatable + environment
	lua_gc(_pState, LUA_GCCOLLECT, 0); // collect garbage!
}



} // namespace Mona

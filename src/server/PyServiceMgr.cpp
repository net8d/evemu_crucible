/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Zhur
*/



#include "EvemuPCH.h"

PyServiceMgr::PyServiceMgr(
	uint32 nodeID, 
	DBcore &db, 
	EntityList &elist, 
	ItemFactory &ifactory)
: item_factory(ifactory),
  entity_list(elist),
  lsc_service(NULL),
  cache_service(NULL),
  m_nextBindID(100),
  m_nodeID(nodeID),
  m_svcDB(&db)
{
	entity_list.UseServices(this);
}

PyServiceMgr::~PyServiceMgr() {
	{
		std::set<PyService *>::iterator cur, end;
		cur = m_services.begin();
		end = m_services.end();
		for(; cur != end; cur++) {
			delete *cur;
		}
	}

	{
		std::map<uint32, BoundObject>::iterator cur, end;
		cur = m_boundObjects.begin();
		end = m_boundObjects.end();
		for(; cur != end; cur++) {
			delete cur->second.destination;
		}
	}
}

void PyServiceMgr::Process() {
	//well... we used to have something to do, but not right now...
}

void PyServiceMgr::RegisterService(PyService *d) {
	m_services.insert(d);
}

PyService *PyServiceMgr::LookupService(const std::string &name) {
	std::set<PyService *>::iterator cur, end;
	cur = m_services.begin();
	end = m_services.end();
	for(; cur != end; cur++) {
		if(name == (*cur)->GetName())
			return(*cur);
	}
	return NULL;
}

PyRepSubStruct *PyServiceMgr::BindObject(Client *c, PyBoundObject *cb, PyRepDict **dict) {
	if(cb == NULL) {
		_log(SERVICE__ERROR, "Tried to bind a NULL object!");
		return(new PyRepSubStruct(new PyRepNone()));
	}

	cb->_SetNodeBindID(GetNodeID(), _GetBindID());	//tell the object what its bind ID is.

	BoundObject obj;
	obj.client = c;
	obj.destination = cb;

	m_boundObjects[cb->bindID()] = obj;

	//_log(SERVICE__MESSAGE, "Binding %s to service %s", bind_str, cb->GetName());

	std::string bind_str = cb->GetBindStr();
	//not sure what this really is...
	uint64 expiration = Win32TimeNow() + Win32Time_Hour;

	PyRepTuple *objt;
	if(dict == NULL || *dict == NULL)
	{
		objt = new PyRepTuple(2);

		objt->items[0] = new PyRepString(bind_str);
		objt->items[1] = new PyRepInteger(expiration);	//expiration?
	}
	else
	{
		objt = new PyRepTuple(3);

		objt->items[0] = new PyRepString(bind_str);
		objt->items[1] = *dict; *dict = NULL;			//consumed
		objt->items[2] = new PyRepInteger(expiration);	//expiration?
	}

	return(new PyRepSubStruct(new PyRepSubStream(objt)));
}

void PyServiceMgr::ClearBoundObjects(Client *who) {
	std::map<uint32, BoundObject>::iterator cur, end;
	cur = m_boundObjects.begin();
	end = m_boundObjects.end();
	while(cur != end) {
		if(cur->second.client == who) {
			//_log(SERVICE__MESSAGE, "Clearing bound object %s", cur->first.c_str());
			cur->second.destination->Release();

			std::map<uint32, BoundObject>::iterator tmp = cur++;
			m_boundObjects.erase(tmp);
		} else {
			cur++;
		}
	}
}

PyBoundObject *PyServiceMgr::FindBoundObject(uint32 bindID) {
	std::map<uint32, BoundObject>::iterator res;
	res = m_boundObjects.find(bindID);
	if(res == m_boundObjects.end())
		return NULL;
	else
		return(res->second.destination);
}

void PyServiceMgr::ClearBoundObject(uint32 bindID) {
	std::map<uint32, BoundObject>::iterator res;
	res = m_boundObjects.find(bindID);
	if(res == m_boundObjects.end()) {
		_log(SERVICE__ERROR, "Unable to find bound object %u to release.", bindID);
		return;
	}
	
	PyBoundObject *bo = res->second.destination;
	//_log(SERVICE__MESSAGE, "Clearing bound object %s (released)", res->first.c_str());

	m_boundObjects.erase(res);
	bo->Release();
}
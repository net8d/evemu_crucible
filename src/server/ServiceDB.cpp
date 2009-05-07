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

ServiceDB::ServiceDB(DBcore *db)
: m_db(db)
{
}

//this is a specialized constructor to help with information hiding:
// if somebody needs to make a new specialized ServiceDB subclass,
// and already has some other ServiceDB subclass which points to the
// correct DB, then it can simply use that ServiceDB as a template
// instead of having to go find the raw dbcore object.
ServiceDB::ServiceDB(ServiceDB *existing_db)
: m_db(existing_db->m_db)
{
}

ServiceDB::~ServiceDB() {}

/**
 * DoLogin()
 *
 * This method performs checks when an account is logging into the server.
 * @param login
 * @param pass
 * @param out_acountID
 * @param out_role
 * @author 
*/
bool ServiceDB::DoLogin(const char *login, const char *pass, uint32 &out_accountID, uint32 &out_role) {
	DBQueryResult res;

	if(pass[0] == '\0') {
		_log(SERVICE__ERROR, "Empty password not allowed (%s)", login);
		return false;
	}

	if(!m_db->IsSafeString(login) || !m_db->IsSafeString(pass)) {
		_log(SERVICE__ERROR, "Invalid characters in login or password!");
		return false;
	}
	
	if(!m_db->RunQuery(res,
		"SELECT accountID,role,password,PASSWORD('%s'),MD5('%s'), online"
		" FROM account"
		" WHERE accountName='%s'", pass, pass, login))
	{
		_log(SERVICE__ERROR, "Error in login query: %s", res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		
		// check if our config is set to auto create the account at login if the account
		// doesn't exist.
		if (!sConfig.server.autoAccount) {
			_log(SERVICE__MESSAGE, "Unknown account '%s'.", login);
			return false;
		} else {
			_log(SERVICE__MESSAGE, "Unknown account '%s'. Let's create a new one.", login);
			if (!CreateNewAccount(login, pass, sConfig.server.autoAccountRole ? sConfig.server.autoAccountRole : 2)) {
				_log(SERVICE__ERROR, "Couldn't create new account '%s'", login);
				return false;
			}
		}

		if(!m_db->RunQuery(res,
			"SELECT accountID,role,password,PASSWORD('%s'),MD5('%s'), online"
			" FROM account"
			" WHERE accountName='%s'", pass, pass, login))
		{
			_log(SERVICE__ERROR, "Error in login query: %s", res.error.c_str());
			return false;
		}

		if(!res.GetRow(row)) {
			_log(SERVICE__MESSAGE, "Failed to create new account '%s'. This is permanent. I'm giving up.", login);
			return false;
		}
	}

	if (row.GetInt(5)) {
		_log(SERVICE__ERROR, "Account '%s' already logged in", login);
		return false;
	}

	if(   strcmp(row.GetText(2), row.GetText(3)) != 0
	   && strcmp(row.GetText(2), row.GetText(4)) != 0
	   && strcmp(row.GetText(2), pass) != 0) {
		_log(SERVICE__ERROR, "Login failed for account '%s'", login);
		return false;
	}
	
	out_accountID = row.GetUInt(0);
	out_role = row.GetUInt(1);
	
	return true;
}

bool ServiceDB::CreateNewAccount( const char * accountName, const char * accountPass, uint64 role )
{
	DBerror err;
	if (!m_db->RunQuery(err,
		"INSERT INTO `account` (accountName,password,role) VALUES ('%s', MD5('%s'), "I64u");",
			accountName, accountPass, role)
	) {
		codelog(SERVICE__ERROR, "Failed to create new account %s: %s", accountName, err.c_str());	
		return false;
	}
	return true;
}

void ServiceDB::SetCharacterLocation(uint32 characterID, uint32 stationID, 
	uint32 systemID, uint32 constellationID, uint32 regionID) {
	DBerror err;
	if(!m_db->RunQuery(err,
		"UPDATE character_"
		" SET"
		"	stationID=%u,"
		"	solarSystemID=%u,"
		"	constellationID=%u,"
		"	regionID=%u"
		" WHERE characterID=%u",
		stationID, systemID, constellationID, regionID,
		characterID)
	) {
		codelog(SERVICE__ERROR, "Failed to set character location %u: %s", characterID, err.c_str());
	}
}

bool ServiceDB::ListEntitiesByCategory(uint32 ownerID, uint32 categoryID, std::vector<uint32> &into) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		"	entity.itemID"
		" FROM entity"
		"	LEFT JOIN invTypes ON entity.typeID=invTypes.typeID"
		"	LEFT JOIN invGroups ON invTypes.groupID=invGroups.groupID"
		" WHERE invGroups.categoryID=%u AND entity.ownerID=%u", categoryID, ownerID))
	{
		_log(SERVICE__ERROR, "Error in ListEntitiesByCategory query: %s", res.error.c_str());
		return false;
	}
	
	DBResultRow row;
	while(res.GetRow(row)) {
		into.push_back(row.GetInt(0));
	}
	return true;
}

uint32 ServiceDB::GetCurrentShipID(uint32 characterID) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		//not sure if this is gunna be valid all the time...
		"SELECT"
		"	locationID"
		" FROM entity"
		" WHERE itemID=%u", characterID
		/*"SELECT "
		"	itemID"
		" FROM entity AS chare LEFT JOIN entity AS shipe"
		"	LEFT JOIN invTypes ON shipe.typeID=invTypes.typeID"
		"	LEFT JOIN invGroups ON invTypes.groupID=invGroups.groupID"
		" WHERE invGroups.categoryID=6 AND 
		" WHERE typeID=%u", typeID*/
	))
	{
		_log(SERVICE__ERROR, "Error in GetCurrentShipID query: %s", res.error.c_str());
		return(0);
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(SERVICE__ERROR, "Error in GetCurrentShipID query: no ship for char id %d", characterID);
		return(0);
	}

	return(row.GetUInt(0));
}

/*
PyRepObject *ServiceDB::GetInventory(uint32 containerID, EVEItemFlags flag) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		" entity.itemID,"
		" entity.typeID,"
		" entity.ownerID,"
		" entity.locationID,"
		" entity.flag,"
		" entity.contraband,"
		" entity.singleton,"
		" entity.quantity,"
		" invTypes.groupID,"
		" invGroups.categoryID,"
		" entity.customInfo"
		" FROM entity "
		"	LEFT JOIN invTypes ON entity.typeID=invTypes.typeID"
		"	LEFT JOIN invGroups ON invTypes.groupID=invGroups.groupID"
		" WHERE entity.locationID=%u "
		"	AND ( %u=0 OR entity.flag=%u)",	//crazy =0 logic is to make 0 match any flag without copying the entire query
			containerID, flag, flag))
	{
		codelog(SERVICE__ERROR, "Error in query for %d,%d: %s", containerID, flag, res.error.c_str());
		return NULL;
	}
	
	return(DBResultToRowset(res));
}
*/


PyRepObject *ServiceDB::GetSolRow(uint32 systemID) const {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		//not sure if this is gunna be valid all the time...
		"SELECT "
		"	itemID,entity.typeID,ownerID,locationID,flag,contraband,singleton,quantity,"
		"	invGroups.groupID, invGroups.categoryID,"
		"	customInfo"
		" FROM entity "
		"	LEFT JOIN invTypes ON entity.typeID=invTypes.typeID"
		"	LEFT JOIN invGroups ON invTypes.groupID=invGroups.groupID"
		" WHERE entity.itemID=%u",
		systemID
	))
	{
		_log(SERVICE__ERROR, "Error in GetSolRow query: %s", res.error.c_str());
		return(0);
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(SERVICE__ERROR, "Error in GetSolRow query: unable to find sol info for system %d", systemID);
		return(0);
	}

	return(DBRowToRow(row, "util.Row"));
}

//this function is temporary, I dont plan to keep this crap in the DB.
PyRepObject *ServiceDB::GetSolDroneState(uint32 systemID) const {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		//not sure if this is gunna be valid all the time...
		"SELECT "
		"	droneID, solarSystemID, ownerID, controllerID,"
		"	activityState, typeID, controllerOwnerID"
		" FROM droneState "
		" WHERE solarSystemID=%u",
		systemID
	))
	{
		_log(SERVICE__ERROR, "Error in GetSolDroneState query: %s", res.error.c_str());
		return NULL;
	}
	
	return(DBResultToRowset(res));
}

bool ServiceDB::GetSystemInfo(uint32 systemID, uint32 *constellationID, uint32 *regionID, std::string *name, std::string *securityClass) {
	if(	   constellationID == NULL
		&& regionID == NULL
		&& name == NULL
		&& securityClass == NULL
	)
		return true;

	DBQueryResult res;
	if(!m_db->RunQuery(res,
		"SELECT"
		" constellationID,"
		" regionID,"
		" solarSystemName,"
		" securityClass"
		" FROM mapSolarSystems"
		" WHERE solarSystemID = %u",
		systemID))
	{
		_log(DATABASE__ERROR, "Failed to query info for system %u: %s.", systemID, res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(DATABASE__ERROR, "Failed to query info for system %u: System not found.", systemID);
		return false;
	}

	if(constellationID != NULL)
		*constellationID = row.GetUInt(0);
	if(regionID != NULL)
		*regionID = row.GetUInt(1);
	if(name != NULL)
		*name = row.GetText(2);
	if(securityClass != NULL)
		*securityClass = row.IsNull(3) ? "" : row.GetText(3);

	return true;
}

bool ServiceDB::GetStaticItemInfo(uint32 itemID, uint32 *systemID, uint32 *constellationID, uint32 *regionID, GPoint *position) {
	if(	   systemID == NULL
		&& constellationID == NULL
		&& regionID == NULL
		&& position == NULL
	)
		return true;

	DBQueryResult res;
	if(!m_db->RunQuery(res,
		"SELECT"
		" solarSystemID,"
		" constellationID,"
		" regionID,"
		" x, y, z"
		" FROM mapDenormalize"
		" WHERE itemID = %u",
		itemID))
	{
		_log(DATABASE__ERROR, "Failed to query info for static item %u: %s.", itemID, res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(DATABASE__ERROR, "Failed to query info for static item %u: Item not found.", itemID);
		return false;
	}

	if(systemID != NULL)
		*systemID = row.GetUInt(0);
	if(constellationID != NULL)
		*constellationID = row.GetUInt(1);
	if(regionID != NULL)
		*regionID = row.GetUInt(2);
	if(position != NULL)
		*position = GPoint(
			row.GetDouble(3),
			row.GetDouble(4),
			row.GetDouble(5)
		);

	return true;
}

bool ServiceDB::GetStationInfo(uint32 stationID, uint32 *systemID, uint32 *constellationID, uint32 *regionID, GPoint *position, GPoint *dockPosition, GVector *dockOrientation) {
	if(	   systemID == NULL
		&& constellationID == NULL
		&& regionID == NULL
		&& position == NULL
		&& dockPosition == NULL
		&& dockOrientation == NULL
	)
		return true;

	DBQueryResult res;
	if(!m_db->RunQuery(res,
		"SELECT"
		" solarSystemID,"
		" constellationID,"
		" regionID,"
		" x, y, z,"
		" dockEntryX, dockEntryY, dockEntryZ,"
		" dockOrientationX, dockOrientationY, dockOrientationZ"
		" FROM staStations"
		" LEFT JOIN staStationTypes USING (stationTypeID)"
		" WHERE stationID = %u",
		stationID))
	{
		_log(DATABASE__ERROR, "Failed to query info for station %u: %s.", stationID, res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(DATABASE__ERROR, "Failed to query info for station %u: Station not found.", stationID);
		return false;
	}

	if(systemID != NULL)
		*systemID = row.GetUInt(0);
	if(constellationID != NULL)
		*constellationID = row.GetUInt(1);
	if(regionID != NULL)
		*regionID = row.GetUInt(2);
	if(position != NULL)
		*position = GPoint(
			row.GetDouble(3),
			row.GetDouble(4),
			row.GetDouble(5)
		);
	if(dockPosition != NULL)
		*dockPosition = GPoint(
			row.GetDouble(3) + row.GetDouble(6),
			row.GetDouble(4) + row.GetDouble(7),
			row.GetDouble(5) + row.GetDouble(8)
		);
	if(dockOrientation != NULL) {
		*dockOrientation = GVector(
			row.GetDouble(9),
			row.GetDouble(10),
			row.GetDouble(11)
		);
		// as it's direction, it should be normalized
		dockOrientation->normalize();
	}

	return true;
}

uint32 ServiceDB::GetDestinationStargateID(uint32 fromSystem, uint32 toSystem) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		" SELECT "
		"    fromStargate.solarSystemID AS fromSystem,"
		"    fromStargate.itemID AS fromGate,"
		"    toStargate.itemID AS toGate,"
		"    toStargate.solarSystemID AS toSystem"
		" FROM mapJumps AS jump"
		" LEFT JOIN mapDenormalize AS fromStargate"
		"    ON fromStargate.itemID = jump.stargateID"
		" LEFT JOIN mapDenormalize AS toStargate"
		"    ON toStargate.itemID = jump.celestialID"
		" WHERE fromStargate.solarSystemID = %u"
		"    AND toStargate.solarSystemID = %u",
		fromSystem, toSystem
	))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return(0);
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Error in query: no data for %d, %d", fromSystem, toSystem);
		return(0);
	}

	return row.GetUInt(2);
}

bool ServiceDB::GetConstant(const char *name, uint32 &into) {
	DBQueryResult res;

	std::string escaped;
	m_db->DoEscapeString(escaped, name);
	
	if(!m_db->RunQuery(res,
	"SELECT"
	"	constantValue"
	" FROM eveConstants"
	" WHERE constantID='%s'",
		escaped.c_str()
	))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return false;
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Unable to find constant %s", name);
		return false;
	}
	
	into = row.GetUInt64(0);
	
	return true;
}

void ServiceDB::ProcessStringChange(const char * key, const std::string & oldValue, const std::string & newValue, PyRepDict * notif, std::vector<std::string> & dbQ) {
	if (oldValue != newValue) {
		std::string newEscValue;
		std::string qValue(key);

		m_db->DoEscapeString(newEscValue, newValue);
		
		// add to notification
		PyRepTuple * val = new PyRepTuple(2);
		val->items[0] = new PyRepString(oldValue);
		val->items[1] = new PyRepString(newValue);
		notif->add(key, val);

		qValue += " = '" + newEscValue + "'";
		dbQ.push_back(qValue);
	}
}

void ServiceDB::ProcessRealChange(const char * key, double oldValue, double newValue, PyRepDict * notif, std::vector<std::string> & dbQ) {
	if (oldValue != newValue) {
		// add to notification
		std::string qValue(key);

		PyRepTuple * val = new PyRepTuple(2);
		val->items[0] = new PyRepReal(oldValue);
		val->items[1] = new PyRepReal(newValue);
		notif->add(key, val);

		char cc[10];
		snprintf(cc, 9, "'%5.3lf'", newValue);
		qValue += " = ";
		qValue += cc;
		dbQ.push_back(qValue);
	}
}

void ServiceDB::ProcessIntChange(const char * key, uint32 oldValue, uint32 newValue, PyRepDict * notif, std::vector<std::string> & dbQ) {
	if (oldValue != newValue) {
		// add to notification
		PyRepTuple * val = new PyRepTuple(2);
		std::string qValue(key);

		val->items[0] = new PyRepInteger(oldValue);
		val->items[1] = new PyRepInteger(newValue);
		notif->add(key, val);

		char cc[10];
		snprintf(cc, 9, "%u", newValue);
		qValue += " = ";
		qValue += cc;
		dbQ.push_back(qValue);
	}
}

//johnsus - characterOnline mod
void ServiceDB::SetCharacterOnlineStatus(uint32 char_id, bool onoff_status) {
	DBerror err;	

	_log(CLIENT__TRACE, "ChrStatus: Setting character %u %s.", char_id, onoff_status ? "Online" : "Offline");

	if(!m_db->RunQuery(err,
		"UPDATE character_"
		" SET online = %d"
		" WHERE characterID = %u",
		onoff_status, char_id))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", err.c_str());
	}
}

//johnsus - serverStartType mod
void ServiceDB::SetServerOnlineStatus(bool onoff_status) {
	DBerror err;

	_log(onoff_status ? SERVER__INIT : SERVER__SHUTDOWN, "SrvStatus: Server is %s, setting serverStartTime.", onoff_status ? "coming Online" : "going Offline");

	if(!m_db->RunQuery(err,
		"REPLACE INTO srvStatus (config_name, config_value)"
		" VALUES ('%s', %s)",
		"serverStartTime",
		onoff_status ? "UNIX_TIMESTAMP(CURRENT_TIMESTAMP)" : "0"))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", err.c_str());
	}

	_log(CLIENT__TRACE, "ChrStatus: Setting all characters and accounts offline.");

	if(!m_db->RunQuery(err,
		"UPDATE character_, account"
		" SET character_.online = 0,"
		"     account.online = 0"))
        {
                codelog(SERVICE__ERROR, "Error in query: %s", err.c_str());
        }
}

void ServiceDB::SetAccountOnlineStatus(uint32 accountID, bool onoff_status) {
	DBerror err;

	_log(CLIENT__TRACE, "AccStatus: Setting account %u %s.", accountID, onoff_status ? "Online" : "Offline");

	if(!m_db->RunQuery(err,
		"UPDATE account "
		" SET account.online = %d "
		" WHERE accountID = %u ",
		onoff_status, accountID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", err.c_str());
	}
}
/*
 * sql.c
 *
 *  Created on: Nov 9, 2016
 *      Author: cody
 */

#include <sqlite3.h>
#include <stdlib.h>
#include <stdio.h>
#include <json-c/json.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <string.h>
#include "util.h"
#include "crypt.h"
#include "sql.h"
#include "1pass.h"

sqlite3* dbHandle = NULL;

int countItems(char* file)
{
	int retval;
	sqlite3* handle;
	sqlite3_stmt* query;
	char* queryFmt;
	
	retval = sqlite3_open_v2(file, &handle, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_open() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}
	
	queryFmt = "SELECT id, category_uuid FROM items ORDER BY category_uuid;";
	
	retval = sqlite3_prepare_v2(handle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}
	
	int counter=0;
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		counter++;
	}
	if(retval != SQLITE_DONE)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}

	retval = sqlite3_finalize(query);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_finalize() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}

	sqlite3_close(handle);
	
	return counter;
}

int printItemJSON(char* file, char* symkey)
{
	int retval;
	sqlite3* handle;
	sqlite3_stmt* query;
	char* queryFmt;
	
	retval = sqlite3_open_v2(file, &handle, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_open() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}
	
	queryFmt = "SELECT id, overview FROM items ORDER BY id;";
	
	retval = sqlite3_prepare_v2(handle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}
	
	printf("items:\n");
	int counter=0;
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		counter++;
		
		int id = sqlite3_column_int(query, 0);
		char* overview = (char*) sqlite3_column_text(query, 1);
		char* pt;
		int ret = decryptItem(&pt, overview);
		
		hexdump("", pt, strlen(pt));
		
		printf("\n");
		
		if(!ret)
		{
			retval = SQLITE_DONE;
			break;
		}
		free(pt);
	}
	if(retval != SQLITE_DONE)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}

	retval = sqlite3_finalize(query);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_finalize() error: %d\n", retval);
		sqlite3_close(handle);
		return -1;
	}

	sqlite3_close(handle);
	
	return counter;
}

int openDB(char* file)
{
	int retval;
	
	retval = sqlite3_open_v2(file, &dbHandle, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_open() error: %d\n", retval);
		sqlite3_close(dbHandle);
		dbHandle = NULL;
		return -1;
	}
	return 0;
}

// TODO: finalize before return

int getMUKsalt(char** p2s, char** alg, int* p2c)
{
//	#define TESTS
	#ifdef TESTS
		*alg = malloc(100);
		*p2s = malloc(100);
		*p2c = 100000;
		strcpy(*alg, "PBES2g-HS256");
		strcpy(*p2s, "1234567890123456789012");
		return 1;
	#endif
	
	// SELECT enc_sym_key FROM keysets WHERE encrypted_by = mp ORDER BY updated_at DESC LIMIT 1
	
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	int found = 0;
	int vid = 0;
	
	queryFmt = "SELECT enc_sym_key FROM keysets WHERE encrypted_by==?001 ORDER BY updated_at DESC LIMIT 1;";

	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}

	retval = sqlite3_bind_text(query, 1, "mp", -1, SQLITE_STATIC);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}

	retval = sqlite3_step(query);
	if(retval != SQLITE_ROW)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		return 0;
	}

	const char* enc_sym_key = (const char*) sqlite3_column_text(query, 0);
	
	struct json_object* jobj = json_tokener_parse(enc_sym_key);
	struct json_object* val;
	json_object_object_get_ex(jobj, "p2s", &val);
	int size = json_object_get_string_len(val);
	*p2s = malloc(size+1);
	if(!*p2s)
	{
		json_object_put(jobj);
		return 0;
	}
	memcpy(*p2s, json_object_get_string(val), size);
	(*p2s)[size] = '\0';
	
	json_object_object_get_ex(jobj, "alg", &val);
	size = json_object_get_string_len(val);
	*alg = malloc(size+1);
	if(!*alg)
	{
		free(*p2s);
		json_object_put(jobj);
		return 0;
	}
	memcpy(*alg, json_object_get_string(val), size);
	(*alg)[size] = '\0';

	json_object_object_get_ex(jobj, "p2c", &val);
	*p2c = json_object_get_int(val);

	json_object_put(jobj);
	
	retval = sqlite3_finalize(query);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_finalize() error: %d\n", retval);
		free(*alg);
		free(*p2s);
		return 0;
	}
	
	return 1;
}

int findKid(char** ctJSON, const char* uuid)
{
	// SELECT id, enc_attrs FROM vaults;
	// match uuid
	// SELECT enc_vault_key FROM vault_access WHERE vault_id = matchedID
	// return enc_vault_key
	
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	int found = 0;
	int vid = 0;
	
	queryFmt = "SELECT id, enc_attrs FROM vaults;";
	
	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}
	
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		vid = sqlite3_column_int(query, 0);
		char* enc = (char*) sqlite3_column_text(query, 1);
		
		struct json_object* jobj = json_tokener_parse(enc);
		struct json_object* kid;
		json_object_object_get_ex(jobj, "kid", &kid);
		
		if(!strcmp(uuid, json_object_get_string(kid)))
		{
			found = 1;
			json_object_put(jobj);
			break;
		}
		
		json_object_put(jobj);
	}
	
	if(retval != SQLITE_DONE && retval != SQLITE_ROW)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		return 0;
	}

	retval = sqlite3_finalize(query);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_finalize() error: %d\n", retval);
		return 0;
	}
	
	if(found)
	{
		queryFmt = "SELECT enc_vault_key FROM vault_access WHERE vault_id==?001;";
		retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		retval = sqlite3_bind_int(query, 1, vid);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n", retval);
			return 0;
		}
		
		char* enc_vault_key = (char*) sqlite3_column_text(query, 0);
		int size = sqlite3_column_bytes(query, 0);
		*ctJSON = malloc(size+1);
		if(!*ctJSON)
		{
			return 0;
		}
		memcpy(*ctJSON, enc_vault_key, size);
		(*ctJSON)[size] = '\0';

		retval = sqlite3_finalize(query);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_finalize() error: %d\n", retval);
			free(*ctJSON);
			return 0;
		}
		return 1;
	}
	
	// Failed, so check elsewhere
	// if !(uuid ends with "-sym")
	// SELECT enc_pri_key FROM keysets WHERE uuid = 'uuid' LIMIT 1;
	// enc_pri_key[kid] -> enc_pri_key[kid]-sym
	// return enc_pri_key
	// else
	// chop suffix from uuid
	// SELECT enc_sym_key FROM keysets WHERE uuid = 'uuid' LIMIT 1;
	// return enc_sym_key
	
	if(strnrcmp(uuid, "-sym", 4))
	{
		queryFmt = "SELECT enc_pri_key FROM keysets WHERE uuid==?001 ORDER BY updated_at DESC LIMIT 1;";
		retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		retval = sqlite3_bind_text(query, 1, uuid, -1, SQLITE_STATIC);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n\n", retval);
			return 0;
		}
		
		const char* enc_pri_key = (const char*) sqlite3_column_text(query, 0);
		
		char uuid[UUIDSTRLEN];
		struct json_object* jobj = json_tokener_parse(enc_pri_key);
		struct json_object* val;
		json_object_object_get_ex(jobj, "kid", &val);
		strncpy(uuid, json_object_get_string(val), UUIDLEN);
		memcpy(uuid+UUIDLEN, "-sym", 5);
		json_object_object_add(jobj, "kid", json_object_new_string(uuid));

		enc_pri_key = json_object_get_string(jobj);
		int size = strlen(enc_pri_key);
		*ctJSON = malloc(size+1);
		if(!*ctJSON)
		{
			json_object_put(jobj);
			return 0;
		}
		memcpy(*ctJSON, enc_pri_key, size);
		(*ctJSON)[size] = '\0';
		json_object_put(jobj);
		
		retval = sqlite3_finalize(query);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_finalize() error: %d\n", retval);
			free(*ctJSON);
			return 0;
		}
		
		return 1;
	}
	else
	{
		char tmpUuid[UUIDSTRLEN];
		memcpy(tmpUuid, uuid, UUIDLEN);
		tmpUuid[UUIDLEN] = '\0';
		queryFmt = "SELECT enc_sym_key FROM keysets WHERE uuid==?001 ORDER BY updated_at DESC LIMIT 1;";
		retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		retval = sqlite3_bind_text(query, 1, tmpUuid, -1, SQLITE_STATIC);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n", retval);
			return 0;
		}
		
		char* enc_sym_key = (char*) sqlite3_column_text(query, 0);
		int size = sqlite3_column_bytes(query, 0);
		*ctJSON = malloc(size+1);
		if(!*ctJSON)
		{
			return 0;
		}
		memcpy(*ctJSON, enc_sym_key, size);
		(*ctJSON)[size] = '\0';
		
		retval = sqlite3_finalize(query);
		if(retval != SQLITE_OK)
		{
			dbgLog("sqlite3_finalize() error: %d\n", retval);
			free(*ctJSON);
			return 0;
		}
		
		return 1;
	}
	
	// TODO: figure out what to do with uuid = 'mp'

	return 0;
}

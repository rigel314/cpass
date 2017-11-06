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
#include <strings.h>
#include "util.h"
#include "crypt.h"
#include "sql.h"
#include "1pass.h"

sqlite3* dbHandle = NULL;

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

struct mkSalt* getMUKsalt()
{
	struct mkSalt* mks = malloc(sizeof(struct mkSalt));
	if(!mks)
		return NULL;
	
	char** p2s = &mks->salt;
	char** alg = &mks->alg;
	int* p2c = &mks->count;
	
//	#define TESTS
	#ifdef TESTS
		*alg = malloc(100);
		*p2s = malloc(100);
		*p2c = 100000;
		strcpy(*alg, "PBES2g-HS256");
		strcpy(*p2s, "1234567890123456789012");
		return mks;
	#endif
	
	// SELECT enc_sym_key FROM keysets WHERE encrypted_by = mp ORDER BY updated_at DESC LIMIT 1
	
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
//	int found = 0;
//	int vid = 0;
	
	queryFmt = "SELECT enc_sym_key FROM keysets WHERE encrypted_by==?001 ORDER BY updated_at DESC LIMIT 1;";

	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		free(mks);
		return NULL;
	}

	retval = sqlite3_bind_text(query, 1, "mp", -1, SQLITE_STATIC);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		sqlite3_finalize(query);
		return 0;
	}

	retval = sqlite3_step(query);
	if(retval != SQLITE_ROW)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		sqlite3_finalize(query);
		free(mks);
		return NULL;
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
		sqlite3_finalize(query);
		free(mks);
		return NULL;
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
		sqlite3_finalize(query);
		free(mks);
		return NULL;
	}
	memcpy(*alg, json_object_get_string(val), size);
	(*alg)[size] = '\0';

	json_object_object_get_ex(jobj, "p2c", &val);
	*p2c = json_object_get_int(val);

	json_object_put(jobj);
	
	sqlite3_finalize(query);
	
	return mks;
}

int freeMUKsalt(struct mkSalt* mks)
{
	free(mks->alg);
	free(mks->salt);
	free(mks);
	
	return 0;
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
		sqlite3_finalize(query);
		return 0;
	}

	sqlite3_finalize(query);
	
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
			sqlite3_finalize(query);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n", retval);
			sqlite3_finalize(query);
			return 0;
		}
		
		char* enc_vault_key = (char*) sqlite3_column_text(query, 0);
		int size = sqlite3_column_bytes(query, 0);
		*ctJSON = malloc(size+1);
		if(!*ctJSON)
		{
			sqlite3_finalize(query);
			return 0;
		}
		memcpy(*ctJSON, enc_vault_key, size);
		(*ctJSON)[size] = '\0';

		sqlite3_finalize(query);

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
			sqlite3_finalize(query);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n\n", retval);
			sqlite3_finalize(query);
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
			sqlite3_finalize(query);
			return 0;
		}
		memcpy(*ctJSON, enc_pri_key, size);
		(*ctJSON)[size] = '\0';
		json_object_put(jobj);
		
		sqlite3_finalize(query);
		
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
			sqlite3_finalize(query);
			return 0;
		}
		
		if((retval = sqlite3_step(query)) != SQLITE_ROW)
		{
			dbgLog("sqlite3_step() error: %d\n", retval);
			sqlite3_finalize(query);
			return 0;
		}
		
		char* enc_sym_key = (char*) sqlite3_column_text(query, 0);
		int size = sqlite3_column_bytes(query, 0);
		*ctJSON = malloc(size+1);
		if(!*ctJSON)
		{
			sqlite3_finalize(query);
			return 0;
		}
		memcpy(*ctJSON, enc_sym_key, size);
		(*ctJSON)[size] = '\0';
		
		sqlite3_finalize(query);
		
		return 1;
	}
	
	return 0;
}

int getCatagories(struct catagory** names)
{
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	
	queryFmt = "SELECT DISTINCT categories.plural_name,categories.uuid FROM items INNER JOIN categories ON items.category_uuid=categories.uuid;";
	
	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}
	
	*names = NULL;
	int rowCount = 0;
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		rowCount++;
		struct catagory* namesBak = *names;
		*names = realloc(*names, rowCount * sizeof(struct catagory));
		if(!*names)
		{
			*names = namesBak;
			rowCount--;
			break;
		}
		struct catagory* cat = (*names)+(rowCount - 1);

		const char* name = sqlite3_column_text(query, 0);
		cat->name = malloc(strlen(name) + 1);
		if(!cat->name)
		{
			rowCount--;
			break;
		}
		memcpy(cat->name, name, strlen(name) + 1);
		
		int id = sqlite3_column_int(query, 1);
		cat->id = id;
	}
	
	sqlite3_finalize(query);
	
	return rowCount;
}

static int sortTags(const void* a, const void* b)
{
	char* ain = *(char**)a;
	char* bin = *(char**)b;
	
	return strcasecmp(ain, bin);
}

int getTags(char*** tags)
{
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	
	queryFmt = "SELECT overview FROM items ORDER BY id;";
	
	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}
	
	int counter=0;
	*tags = NULL;
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		char* overview = (char*) sqlite3_column_text(query, 0);
		char* pt = NULL;
		int ret = decryptItem(&pt, overview);
		
		if(pt && ret)
		{
			struct json_object* jobj = json_tokener_parse(pt);
			json_object_object_foreach(jobj, name, val)
			{
				if(!strcmp("tags", name) && json_object_is_type(val, json_type_array))
				{
					int len = json_object_array_length(val);
					for(int i = 0; i < len; i++)
					{
						struct json_object* arr = json_object_array_get_idx(val, i);
						const char* rowTag = json_object_get_string(arr);
						int new = 1;
						for(int j = 0; j < counter; j++)
						{
							if(!strcmp(rowTag, (*tags)[j]))
							{
								new = 0;
								break;
							}
						}
						if(!new)
							continue;
						
						counter++;
						char** tagsBak = *tags;
						*tags = realloc(*tags, sizeof(char*) * counter);
						if(!*tags)
						{
							counter--;
							*tags = tagsBak;
							break;
						}
						(*tags)[counter-1] = malloc(strlen(rowTag) + 1);
						if(!(*tags)[counter-1])
						{
							counter--;
							break;
						}
						strcpy((*tags)[counter-1], rowTag);
					}
				}
			}
			json_object_put(jobj);
			free(pt);
		}
		else
		{
			break;
		}
	}

	sqlite3_finalize(query);

	if(counter > 0)
		qsort(*tags, counter, sizeof(char**), sortTags);
	
	return counter;
}

static int sortItems(const void* a, const void* b)
{
	struct item* ain = (struct item*)a;
	struct item* bin = (struct item*)b;
	
	return strcasecmp(ain->name, bin->name);
}

int getItems(struct item** items)
{
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	
	queryFmt = "SELECT overview,id FROM items WHERE trashed==0;";
	
	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}
	
	*items = NULL;
	int rowCount = 0;
	while((retval = sqlite3_step(query)) == SQLITE_ROW)
	{
		rowCount++;
		struct item* namesBak = *items;
		*items = realloc(*items, rowCount * sizeof(struct catagory));
		if(!*items)
		{
			*items = namesBak;
			rowCount--;
			break;
		}
		struct item* item = (*items)+(rowCount - 1);

		char* overview = (char*) sqlite3_column_text(query, 0);
		char* pt = NULL;
		int ret = decryptItem(&pt, overview);
		if(pt && ret)
		{
			struct json_object* jobj = json_tokener_parse(pt);
			json_object_object_foreach(jobj, name, val)
			{
				if(!strcmp("title", name) && json_object_is_type(val, json_type_string))
				{
					const char* title = json_object_get_string(val);
					item->name = malloc(strlen(title) + 1);
					if(!item->name)
					{
						rowCount--;
						break;
					}
					memcpy(item->name, title, strlen(title) + 1);
					
					int id = sqlite3_column_int(query, 1);
					item->id = id;
				}
			}
			json_object_put(jobj);
			free(pt);
		}
		else
		{
			rowCount--;
			break;
		}
	}
	
	sqlite3_finalize(query);
	
	if(rowCount>0)
		qsort(*items, rowCount, sizeof(struct item), sortItems);
	
	return rowCount;
}

int getItemByID(char** ptJSON, int id)
{
	int retval;
	sqlite3_stmt* query;
	char* queryFmt;
	
	queryFmt = "SELECT details FROM items WHERE id==?001;";
	
	retval = sqlite3_prepare_v2(dbHandle, queryFmt, -1, &query, NULL);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_prepare_v2() error: %d\n", retval);
		return 0;
	}
	
	retval = sqlite3_bind_int(query, 1, id);
	if(retval != SQLITE_OK)
	{
		dbgLog("sqlite3_bind_int() error: %d\n", retval);
		return 0;
	}
	
	retval = sqlite3_step(query);
	if(retval != SQLITE_ROW)
	{
		dbgLog("sqlite3_step() error: %d\n", retval);
		return 0;
	}

	char* details = (char*) sqlite3_column_text(query, 0);
	*ptJSON = NULL;
	int ret = decryptItem(ptJSON, details);
	if(!(*ptJSON && ret))
		dbgLog("decryptItem() failed decrypting item.\n");
	
	sqlite3_finalize(query);
	
	return 1;
}

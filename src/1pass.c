/*
 * 1pass.c
 *
 *  Created on: Dec 20, 2016
 *      Author: cody
 */

#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
#include <json-c/json.h>
#include "1pass.h"
#include "util.h"
#include "crypt.h"
#include "sql.h"

struct keys* keys = NULL;

int addKey(struct keys** keylist, const char* uuid, char* key, enum keyType type, char** keyout)
{
	if(!keylist)
		return 1;

	struct keys* new = malloc(sizeof(struct keys));
	new->next = NULL;
	new->type = type;
	strncpy(new->kid, uuid, 27);
	switch(type)
	{
		case KT_aes:
			strncpy(new->aes, key, 32);
			if(keyout)
				*keyout = new->aes;
			new->rsaJSON = NULL;
			break;
		case KT_rsa:
			new->rsaJSON = malloc(strlen(key)+1);
			strcpy(new->rsaJSON, key);
			if(keyout)
				*keyout = new->rsaJSON;
			break;
		default:
			free(new);
			return 2;
			break;
	}
	
	if(!*keylist)
	{
		*keylist = new;
		return 0;
	}
	
	struct keys* ptr;
	for(ptr = *keylist; ptr->next != NULL; ptr=ptr->next);
	
	ptr->next = new;
	
	return 0;
}

void printUUIDs(struct keys** keylist)
{
	LLforeach(struct keys*, ptr, keys)
	{
		dbgLog("%d, %s\n", ptr->type, ptr->kid);
		if(ptr->type == KT_aes)
					hexdump("", ptr->aes, 32);
		if(ptr->type == KT_rsa)
					hexdump("", ptr->rsaJSON, strlen(ptr->rsaJSON));
	}
}

int freeKeys(struct keys** keylist)
{
	if(!keylist || !*keylist)
		return 0;
	freeKeys(&(*keylist)->next);
	if((*keylist)->rsaJSON)
	{
		OPENSSL_cleanse((*keylist)->rsaJSON, strlen((*keylist)->rsaJSON));
		free((*keylist)->rsaJSON);
	}
	OPENSSL_cleanse((*keylist)->aes, 32);
	free(*keylist);
	*keylist = NULL;
	return 0;
}

enum keyType getKeytype(const char* name)
{
	if(!strcmp(name, "A256GCM"))
		return KT_aes;
	if(!strcmp(name, "RSA-OAEP"))
		return KT_rsa;
	return KT_unknown;
}

int decryptKey(char**ptJSON, char* ctJSON)
{
	char* key;
	int ret = 0;
	
	struct json_object* jobj = json_tokener_parse(ctJSON);

	struct json_object* enc;
	json_object_object_get_ex(jobj, "enc", &enc);

	struct json_object* kid;
	json_object_object_get_ex(jobj, "kid", &kid);
	
	struct json_object* data;
	json_object_object_get_ex(jobj, "data", &data);
	char* ct = malloc(base64contraction(json_object_get_string_len(data))+1);
	char* pt = malloc(base64contraction(json_object_get_string_len(data))+1);
	int ctlen;
	int ptlen;
	ctlen = base64dec(ct, json_object_get_string_len(data), json_object_get_string(data));

	getKeyByUUID(&key, json_object_get_string(kid));
	if(key)
	{
		switch(getKeytype(json_object_get_string(enc)))
		{
			case KT_aes:
				voidStatment;
				struct json_object* iv;
				json_object_object_get_ex(jobj, "iv", &iv);
				char ivraw[20];
				base64dec(ivraw, json_object_get_string_len(iv), json_object_get_string(iv));
				
				ret = aes256gcmdec(pt, &ptlen, ct, ctlen, key, ivraw);
				pt[ptlen] = '\0';
				*ptJSON = pt;
				
	//			json_object_put(iv);
				break;
				
			case KT_rsa:
				ret = rsaoaepdec(pt, &ptlen, ct, ctlen, key);
				pt[ptlen] = '\0';
				*ptJSON = pt;
				break;
			
			default:
				free(pt);
				free(ct);
				break;
		}
	}
	else
	{
		dbgLog("failed finding key: %s\n", json_object_get_string(kid));
	}

	free(ct);
//	json_object_put(data);
//	json_object_put(enc);
//	json_object_put(kid);
	json_object_put(jobj);
	
	return ret;
}

int getKeyByUUID(char** out, const char* uuid)
{
	dbgLog("%s\n", uuid);
	LLforeach(struct keys*, ptr, keys)
	{
		if(!strcmp(uuid, ptr->kid))
		{
			switch(ptr->type)
			{
				case KT_aes:
					*out = ptr->aes;
					return 32;
					break;
				case KT_rsa:
					*out = ptr->rsaJSON;
					return strlen(*out);
					break;
			}
		}
	}
	
	char* ctJSON;
	char* ptJSON;
	*out = NULL;
	if(findKid(&ctJSON, uuid))
	{
		int ret = decryptKey(&ptJSON, ctJSON);
		if(ret)
		{
			struct json_object* jobj = json_tokener_parse(ptJSON);

			struct json_object* alg;
			json_object_object_get_ex(jobj, "alg", &alg);
			switch(getKeytype(json_object_get_string(alg)))
			{
				case KT_aes:
					voidStatment;
					struct json_object* k;
					json_object_object_get_ex(jobj, "k", &k);
					addKey(&keys, uuid, ptJSON, KT_aes, out);
					ret = base64dec(*out, json_object_get_string_len(k), json_object_get_string(k));
					json_object_put(k);
					break;
				case KT_rsa:
					addKey(&keys, uuid, ptJSON, KT_rsa, out);
					ret = strlen(ptJSON);
					break;
				default:
					ret = 0;
					break;
			}
//			json_object_put(alg);
			json_object_put(jobj);
			free(ptJSON);
		}
		else
		{
			dbgLog("Failed to decrypt key: %s\n", uuid);
		}
		free(ctJSON);
		return ret;
	}
	return 0;
}

int decryptItem(char** ptJSON, char* ctJSON)
{
	return decryptKey(ptJSON, ctJSON);
}

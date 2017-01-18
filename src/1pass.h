/*
 * 1pass.h
 *
 *  Created on: Dec 20, 2016
 *      Author: cody
 */

#ifndef SRC_1PASS_H_
#define SRC_1PASS_H_

#define UUIDLEN 26
#define UUIDSTRLEN 32

enum keyType {KT_unknown, KT_aes, KT_rsa};

struct keys
{
	struct keys* next;
	enum keyType type;
	char kid[UUIDSTRLEN];
	char aes[32];
	char* rsaJSON;
};

extern struct keys* keys;

int addKey(struct keys** keylist, const char* uuid, char* key, enum keyType type, char** keyout);
int getKeyByUUID(char** out, const char* uuid);
int decryptItem(char** ptJSON, char* ctJSON);
void printUUIDs(struct keys** keylist);

#endif /* SRC_1PASS_H_ */

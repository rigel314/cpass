/*
 * sql.h
 *
 *  Created on: Nov 10, 2016
 *      Author: cody
 */

#ifndef SRC_SQL_H_
#define SRC_SQL_H_

#include <sqlite3.h>

int countItems(char* file);
int printItemJSON(char* file, char* symkey);
int openDB(char* file);
int getMUKsalt(char** p2s, char** alg, int* p2c);
int findKid(char** ctJSON, const char* uuid);

extern sqlite3* dbHandle;

extern char accountKey[100];
extern char email[];
extern char id[];


#endif /* SRC_SQL_H_ */

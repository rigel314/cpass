/*
 * main.c
 *
 *  Created on: Nov 9, 2016
 *      Author: cody
 */

#include "sql.h"
#include <stdio.h>
#include "util.h"
#include <string.h>
#include "crypt.h"
#include "1pass.h"
#include <openssl/crypto.h>

// TODO: Add some kind of UI
// TODO: Connect to browser extension
// TODO: check json tokener parsing
// TODO: check mallocs

#ifndef TESTS
int main(int argc, char** argv)
{
	if(argc != 2)
		return 1;
	
	char km[32];
	char pass[100] = "aoeu things";
	char accountKey[100] = ""; // get from user
	char email[] = "pi.rubiks@gmail.com"; // get from user
	char id[] = "ASWWYB";
	char salt[100] = ""; // get from keysets
	FILE* fp = fopen("./enc/1passwordKey.txt", "r");
	if(fp)
	{
		char tmp[100];
		fgets(tmp, 100, fp);
		for(int i = 0, j = 0; i < strlen(tmp); i++)
		{ // First line account key
			if(tmp[i] == '\n' || tmp[i] == '\r')
				break;
			if(tmp[i] != '-')
				accountKey[j++] = tmp[i];
		}
		memcpy(id, accountKey+2, 6);
		fgets(tmp, 100, fp);
		for(int i = 0, j = 0; i < strlen(tmp); i++)
		{ // Second line master password
			if(tmp[i] == '\n' || tmp[i] == '\r')
				break;
			pass[j++] = tmp[i];
		}
		fclose(fp);
		OPENSSL_cleanse(tmp, 100);
	}

	openDB(argv[1]);

	masterKey(km, pass, accountKey+8, email, id);
	
	addKey(&keys, "mp", km, KT_aes, NULL);

	printItemJSON(argv[1], NULL);
	
	return 0;
}

#else
int main()
{
	int ret = 0;
	ret += testHkdf();
	ret += testBase64();
	ret += testStrnrcmp();
	printf("%d tests failed\n", ret);
	return ret;
}
#endif

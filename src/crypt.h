/*
 * crypt.h
 *
 *  Created on: Nov 27, 2016
 *      Author: cody
 */

#ifndef SRC_CRYPT_H_
#define SRC_CRYPT_H_

int masterKey(char* km, char* p, char* ka, char* e, char* I);
int hkdf(char* okm, int olen, char* salt, int slen, char* key, int klen, char* info, int ilen);
int aes256gcmdec(char* pt, int* ptlen, const char* ct, int ctlen, const char* key, const char* iv);
int rsaoaepdec(char* pt, int* ptlen, char* ct, int ctlen, char* key);

#ifdef TESTS
int testHkdf();
#endif

#endif /* SRC_CRYPT_H_ */

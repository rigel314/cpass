/*
 * crypt.h
 *
 *  Created on: Nov 27, 2016
 *      Author: cody
 */

#ifndef SRC_CRYPT_H_
#define SRC_CRYPT_H_

struct mkSalt
{
	char* salt; // Actual salt value
	char* alg; // "PBES2g-HS256"
	int count; // 100000
};

int masterKey(char* km, char* p, char* ka, char* e, char* I, struct mkSalt salt);
int hkdf(char* okm, int olen, char* salt, int slen, char* key, int klen, char* info, int ilen);
int aes256gcmdec(char* pt, int* ptlen, const char* ct, int ctlen, const char* key, const char* iv);
int rsaoaepdec(char* pt, int* ptlen, char* ct, int ctlen, char* key);
int genSessKey(char* seskey, char* B, int Blen, char* a, int alen, char* sesid, int sesidlen, char* x);

#ifdef TESTS
int testHkdf();
int testMasterKey();
int testSessKey();
#endif

#endif /* SRC_CRYPT_H_ */

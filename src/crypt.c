/*
 * crypt.c
 *
 *  Created on: Nov 9, 2016
 *      Author: cody
 */

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <string.h>
#include <json-c/json.h>
#include "crypt.h"
#include "util.h"
#include "sql.h"

#define IDlen 6
#define ACCOUNTKEYlen 26
#define ACCOUNTKEYver "A3"
#define ACCOUNTKEYverLen 2
#define MUKkeyLen 32

/**
 * returns master unlock key in km
 * takes:
 *	master password as p, null terminated
 *	account key as ka, ACCOUNTKEYlen(26) bytes long
 *	email address as e, null terminated
 *	ID as I, IDlen(6) bytes long
 */
int masterKey(char* km, char* p, char* ka, char* e, char* I)
{
	char s2[100];
	
	char* s;
	char* alg; // "PBES2g-HS256"
	int p2c; // 100000
	int ret = getMUKsalt(&s, &alg, &p2c);
	if(!ret)
		return 0;
	
	int slen = base64dec(s2, strlen(s), s);
	hkdf(s2, SHA256_DIGEST_LENGTH, e, strlen(e), s2, slen, alg, 12);
	PKCS5_PBKDF2_HMAC(p, strlen(p), s2, SHA256_DIGEST_LENGTH, p2c, EVP_sha256(), MUKkeyLen, km);
	hkdf(ka, MUKkeyLen, I, IDlen, ka, ACCOUNTKEYlen, ACCOUNTKEYver, ACCOUNTKEYverLen);
	for(int i = 0; i < MUKkeyLen; i++)
		km[i] = km[i] ^ ka[i];
	
	free(s);
	free(alg);
	OPENSSL_cleanse(s2, SHA256_DIGEST_LENGTH);
	OPENSSL_cleanse(s, strlen(s));
	OPENSSL_cleanse(ka, max(ACCOUNTKEYlen, MUKkeyLen));
	OPENSSL_cleanse(p, strlen(p));
	
	return 0;
}

/**
 * returns output keying material in okm, olen bytes long
 * takes:
 *	salt as salt, slen bytes long
 *	key as key, klen bytes long
 *	info as info, ilen bytes long
 */
int hkdf(char* okm, int olen, char* salt, int slen, char* key, int klen, char* info, int ilen)
{
	char prk[SHA256_DIGEST_LENGTH];
	int steps = (olen / SHA256_DIGEST_LENGTH) + (olen % SHA256_DIGEST_LENGTH != 0);
	char T[SHA256_DIGEST_LENGTH];
	char Tprime[SHA256_DIGEST_LENGTH+ilen+1];
	int TprimeLen = 0;
	int offset=0, cpyLen=0;
	
	HMAC(EVP_sha256(), salt, slen, key, klen, prk, NULL);
	
	for(int i = 0; i < steps; i++)
	{
		if(i > 0)
		{
			memcpy(Tprime, T, SHA256_DIGEST_LENGTH);
			memcpy(Tprime+SHA256_DIGEST_LENGTH, info, ilen);
			Tprime[SHA256_DIGEST_LENGTH + ilen] = i+1;
			TprimeLen = SHA256_DIGEST_LENGTH + ilen + 1;
		}
		else
		{
			memcpy(Tprime, info, ilen);
			Tprime[ilen] = i+1;
			TprimeLen = ilen + 1;
		}
		
//		printf("round: %d\n", i);
		HMAC(EVP_sha256(), prk, SHA256_DIGEST_LENGTH, Tprime, TprimeLen, T, NULL);
		
		if(offset + SHA256_DIGEST_LENGTH > olen)
			cpyLen = olen - offset;
		else
			cpyLen = SHA256_DIGEST_LENGTH;
		
		memcpy(okm + offset, T, cpyLen);
		offset += SHA256_DIGEST_LENGTH;
	}
	
	OPENSSL_cleanse(prk, SHA256_DIGEST_LENGTH);
	OPENSSL_cleanse(T, SHA256_DIGEST_LENGTH);
	OPENSSL_cleanse(Tprime, SHA256_DIGEST_LENGTH+ilen+1);
	
	return 0;
}

/**
 * returns plain text in pt and length of plain text in ptlen
 * takes:
 *	cipher text as ct, ctlen bytes long
 *	key as key, 32 bytes long
 *	initial value as iv, 12 bytes long
 * returns FALSE on failure
 */
int aes256gcmdec(char* pt, int* ptlen, const char* ct, int ctlen, const char* key, const char* iv)
{
	int len;
	int ret;
	if(ctlen < 16)
		return 0;
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if(!ctx)
		return 0;

	ret = EVP_DecryptInit(ctx, EVP_aes_256_gcm(), key, iv);
	ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)(ct + ctlen - 16));
	ret = EVP_DecryptUpdate(ctx, pt, &len, ct, ctlen-16);
	ret = EVP_DecryptFinal(ctx, pt+len, ptlen);
	EVP_CIPHER_CTX_free(ctx);
	*ptlen += len;
	return ret;
}

/**
 * returns plain text in pt, and length of plain text in ptlen
 * takes:
 *	cipher text as ct, ctlen bytes long
 *	the private key in JSON form with with base64url encoded values for keys n, d, p, q, dp, dq, qi, and e
 * returns FALSE on failure
 */
int rsaoaepdec(char* pt, int* ptlen, char* ct, int ctlen, char* keyJSON)
{
	// Thank you stack overflow: http://stackoverflow.com/questions/33101829/openssl-how-to-create-rsa-structure-with-p-q-and-e
	RSA* rsa = RSA_new();
	
	char* keys[] = {"n", "d", "p", "q", "dp", "dq", "qi", "e"};
	BIGNUM** bns[] = {&rsa->n, &rsa->d, &rsa->p, &rsa->q, &rsa->dmp1, &rsa->dmq1, &rsa->iqmp, &rsa->e};
	
	enum json_tokener_error err;
	struct json_object* jobj = json_tokener_parse_verbose(keyJSON, &err);
	if(err != json_tokener_success)
		dbgLog("json tokener err: %d\n", err);
	struct json_object* val;
	for(int i = 0; i < 8; i++)
	{
		json_object_object_get_ex(jobj, keys[i], &val);
		char num[1000];
		int len = base64dec(num, json_object_get_string_len(val), json_object_get_string(val));
		*bns[i] = BN_new();
		BN_bin2bn(num, len, *bns[i]);
		showErrs();
	}
//	while(!json_object_put(val));
	json_object_put(jobj);
	
	if(!RSA_check_key(rsa))
	{
		printf("Invalid Private Key\n");
		*ptlen = 0;
		return 0;
	}
	
	*ptlen = RSA_private_decrypt(ctlen, ct, pt, rsa, RSA_PKCS1_OAEP_PADDING);
	if(*ptlen < 0)
	{
		showErrs();
	}
	return 1;
}

//#define TESTS
#ifdef TESTS
int testHkdf()
{
	char hkdftestOut[100];
	char hkdftestSalt[100] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c};
	char hkdftestKey[100] = {0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b};
	char hkdftestInfo[100] = {0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9};
	char tv2hkdftestSalt[100] = {0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
	char tv2hkdftestKey[100] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
	char tv2hkdftestInfo[100] = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
	int ret = 0;
	
//	printf("Compare online at: https://tools.ietf.org/html/rfc5869\n");

	printf("HKDF tests:\n");
	
	char tv1ans[] = {0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65};
	hkdf(hkdftestOut, 42, hkdftestSalt, 13, hkdftestKey, 22, hkdftestInfo, 10);
	ret += testMemcmp("tv1", tv1ans, hkdftestOut, 42);
	
	char tv2ans[] = {0xb1, 0x1e, 0x39, 0x8d, 0xc8, 0x03, 0x27, 0xa1, 0xc8, 0xe7, 0xf7, 0x8c, 0x59, 0x6a, 0x49, 0x34, 0x4f, 0x01, 0x2e, 0xda, 0x2d, 0x4e, 0xfa, 0xd8, 0xa0, 0x50, 0xcc, 0x4c, 0x19, 0xaf, 0xa9, 0x7c, 0x59, 0x04, 0x5a, 0x99, 0xca, 0xc7, 0x82, 0x72, 0x71, 0xcb, 0x41, 0xc6, 0x5e, 0x59, 0x0e, 0x09, 0xda, 0x32, 0x75, 0x60, 0x0c, 0x2f, 0x09, 0xb8, 0x36, 0x77, 0x93, 0xa9, 0xac, 0xa3, 0xdb, 0x71, 0xcc, 0x30, 0xc5, 0x81, 0x79, 0xec, 0x3e, 0x87, 0xc1, 0x4c, 0x01, 0xd5, 0xc1, 0xf3, 0x43, 0x4f, 0x1d, 0x87};
	hkdf(hkdftestOut, 82, tv2hkdftestSalt, 80, tv2hkdftestKey, 80, tv2hkdftestInfo, 80);
	ret += testMemcmp("tv2", tv2ans, hkdftestOut, 82);

	char tv3ans[] = {0x8d, 0xa4, 0xe7, 0x75, 0xa5, 0x63, 0xc1, 0x8f, 0x71, 0x5f, 0x80, 0x2a, 0x06, 0x3c, 0x5a, 0x31, 0xb8, 0xa1, 0x1f, 0x5c, 0x5e, 0xe1, 0x87, 0x9e, 0xc3, 0x45, 0x4e, 0x5f, 0x3c, 0x73, 0x8d, 0x2d, 0x9d, 0x20, 0x13, 0x95, 0xfa, 0xa4, 0xb6, 0x1a, 0x96, 0xc8};
	hkdf(hkdftestOut, 42, hkdftestSalt, 0, hkdftestKey, 22, hkdftestInfo, 0);
	ret += testMemcmp("tv3", tv3ans, hkdftestOut, 42);
	
	return ret;
}

int testMasterKey()
{
	int ret = 0;
	
	printf("Master Unlock Key tests:\n");
	
	char km[MUKkeyLen];
	char pass[] = "blar aoeu";
	char accKey[] = "A3ABCDEF12345678901234567890123456";
	char ka[ACCOUNTKEYlen];
	char email[] = "aoeu@example.com";
	char id[IDlen];
	char truthMUK[MUKkeyLen] = {0x3f, 0xd4, 0xea, 0x15, 0x5e, 0xde, 0x5d, 0x96, 0x8c, 0x5d, 0x8d, 0xb2, 0xf0, 0x8a, 0x8b, 0xba, 0x7e, 0x65, 0xc1, 0xd1, 0x42, 0x6a, 0xf3, 0x6a, 0xe7, 0xc7, 0xdb, 0x8b, 0xfc, 0x7e, 0x5f, 0xaa};
	memcpy(ka, accKey+8, ACCOUNTKEYlen);
	memcpy(id, accKey+2, IDlen);
	
	masterKey(km, pass, ka, email, id);
//	for(int i = 0; i < MUKkeyLen; i++)
//	{
//		printf("%#02x, ", (unsigned char) km[i]);
//	}
//	printf("\n");
	
	ret += testMemcmp("masterKey test", km, truthMUK, MUKkeyLen);
	
	return ret;
}
#endif

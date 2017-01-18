/*
 * util.c
 *
 *  Created on: Nov 24, 2016
 *      Author: cody
 */

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>
#include "util.h"

inline int base64expansion(int len)
{
	return (len/3 + (len%3 != 0)) * 4;
}
inline int base64contraction(int len)
{
	return (len/4 + (len%4 != 0)) * 3;
}

static int b64url_new(BIO *b);
static int b64url_write(BIO *b, const char *in, int len);
static int b64url_read(BIO *b, char *out, int len);
static long b64url_ctrl(BIO *b, int cmd, long num, void *ptr);

static BIO_METHOD methods_b64url = {
	0x0200, "base64url to base64 filter",
	b64url_write,
	b64url_read,
	NULL,
	NULL,
	b64url_ctrl,
	b64url_new,
	NULL,
	NULL,
};

BIO_METHOD* BIO_f_b64url()
{
	return &methods_b64url;
}

static int b64url_new(BIO *b)
{
	b->init = 1;
	return 1;
}
static int b64url_write(BIO *b, const char *in, int len)
{
	char tmp;
	int outlen = 0;
	int ret = 0;
	
	for(int i = 0; i < len; i++)
	{
		tmp = in[i];
		if(tmp == '-')
			tmp = '+';
		if(tmp == '_')
			tmp = '/';
		ret = BIO_write(b->next_bio, &tmp, 1);
		if(ret < 0)
			break;
	}
	
	if(outlen > 0)
		return outlen;
	else
		return ret;
}

static int b64url_read(BIO *b, char *out, int len)
{
	char tmp;
	int outlen = 0;
	int ret = 0;
	
	for(int i = 0; i < len; i++)
	{
		ret = BIO_read(b->next_bio, &tmp, 1);
		if(ret < 0)
			break;
		outlen += ret;
		
		if(tmp == '-')
			tmp = '+';
		if(tmp == '_')
			tmp = '/';
		out[i] = tmp;
	}
	if(outlen > 0)
		return outlen;
	else
		return ret;
}
static long b64url_ctrl(BIO *b, int cmd, long num, void *ptr)
{
	return 0;
}

int base64enc(char* b64, int len, const char* pt)
{
	BIO* bio_64 = BIO_new(BIO_f_base64());
	BIO* bio_mem = BIO_new(BIO_s_mem());
	BIO_push(bio_64, bio_mem);
	
	BIO_set_flags(bio_64, BIO_FLAGS_BASE64_NO_NL);
	
	BIO_write(bio_64, pt, len);
	BIO_flush(bio_64);
	int lenOut = base64expansion(len);
	int ret = BIO_read(bio_mem, b64, lenOut);
	
	BIO_free_all(bio_64);
	
	return ret;
}

int base64dec(char* pt, int len, const char* b64)
{
	BIO* bio_64url = BIO_new(BIO_f_b64url());
	BIO* bio_64 = BIO_new(BIO_f_base64());
	BIO* bio_mem = BIO_new(BIO_s_mem());
	BIO_push(bio_64url, bio_mem);
	BIO_push(bio_64, bio_64url);
	
	BIO_set_flags(bio_64, BIO_FLAGS_BASE64_NO_NL);
	
	BIO_write(bio_mem, b64, len);
	char pad[3] = "==";
	BIO_write(bio_mem, pad, 4*(len%4!=0)-(len%4));
	int lenOut = base64contraction(len);
	int ret = BIO_read(bio_64, pt, lenOut);

	BIO_free_all(bio_64);
	
	return ret;
}

char printingQ(char in);
inline char printingQ(char in)
{
	if(in < ' ' || in > '~')
	{
		in = '.';
	}
	return in;
}

void showErrQueue(char* file, int line)
{
	unsigned long e;
	const char* f;
	int l;
	const char* data;
	int flags;
	while((e = ERR_get_error_line_data(&f, &l, &data, &flags)))
	{
		printf("%s:%d \"%s:%d\"(%d: %s) (%lu) %s\n", file, line, f, l, flags, flags&ERR_TXT_STRING ? data : "", e, ERR_error_string(e, NULL));
	}
}

/**
 * Like strncmp except this compares the end of strings
 * Does not handle NULL inputs
 * Returns an integer less than, equal to, or greater than zero if s1 is found, respectively, to be less than, to match, or be greater than s2.
 * When s1 is shorter than len and s2, returns -1
 * When s2 is shorter than len and s1, returns 1
 */
int strnrcmp(const char* s1, const char* s2, int len)
{
	int s1len = strlen(s1);
	int s2len = strlen(s2);
	
	for(int i = 1; i <= len; i++)
	{
		if(s1len - i < 0 && s2len - i < 0)
			return 0;
		if(s1len - i < 0)
			return -1;
		if(s2len - i < 0)
			return 1;
		int cmp = s1[s1len-i]-s2[s2len-i];
		if(cmp)
			return cmp;
	}
	
	return 0;
}

void dump(const char* message, const char* data, int len, char* format, int rowSize, char* whiteSpace)
{
	printf("%s", message);
	for(int i = 0; i < len/rowSize + (len%rowSize != 0); i++)
	{
		printf("%06x: ", i*rowSize);
		for(int j = 0; j < rowSize; j++)
		{
			if(i*rowSize+j < len)
				printf(format, (unsigned char) data[rowSize*i + j]);
			else
				printf("%s", whiteSpace);
		}
		printf(" |");
		for(int j = 0; j < rowSize && i*rowSize+j < len; j++)
		{
			printf("%c", printingQ((unsigned char) data[rowSize*i + j]));
		}
		printf("|");
		printf("\n");
	}
	printf("len: %d\n", len);
}

void hexdump(const char* message, const char* data, int len)
{
	dump(message, data, len, "%02x ", 16, "   ");
}

void decdump(const char* message, const char* data, int len)
{
	dump(message, data, len, "%03d ", 10, "    ");
}

#ifndef TESTS
//#define TESTS
#endif
#ifdef TESTS
int testBase64()
{
	int ret = 0;
	char in[300] = "hello, this is a test.";
	char encoded[1000];
	char decoded[1000];
	
	int len = strlen(in);
	for(int i = len; i < len+256; i++)
		in[i] = i-len;
	int b64len = base64enc(encoded, len+256, in);
	base64dec(decoded, b64len, encoded);
	
	printf("Base64 tests:\n");
	
	testMemcmp("dec(enc(x))=x", in, decoded, len+256);
	
	return ret;
}

int testStrnrcmp()
{
	int ret = 0;
	
	printf("strnrcmp tests:\n");
	
	ret += testIntcmp("equal", strnrcmp("testing", "blaring", 3), 0);
	ret += testIntcmp("less", strnrcmp("testing", "blaring", 4), 2);
	ret += testIntcmp("greater", strnrcmp("blaring", "testing", 4), -2);
	ret += testIntcmp("smallequal", strnrcmp("ing", "ing", 3), 0);
	ret += testIntcmp("diffequal", strnrcmp("aoeuing", "uing", 3), 0);
	ret += testIntcmp("diffgreater", strnrcmp("aoeuing", "uing", 7), 1);
	ret += testIntcmp("diffless", strnrcmp("uing", "aoeuing", 7), -1);
	return ret;
}

void printPassFail(char* msg, int cmp)
{
	printf("%s: %s\n", msg, cmp ? ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "PASS" ANSI_COLOR_RESET);
}

int testIntcmp(char* msg, int truth, int computed)
{
	int cmp = !(truth == computed);
	printPassFail(msg, cmp);
	return cmp != 0;
}

int testMemcmp(char* msg, char* truth, char* computed, int len)
{
	int cmp = memcmp(truth, computed, len);
	printPassFail(msg, cmp);
	return cmp != 0;
}
#endif

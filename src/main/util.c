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
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <errno.h>
#include <dirent.h>
#include <curl/curl.h>
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

int mkdirp(char* dir)
{
	char* tmp = dir;
	if(*tmp == '/')
		tmp++;
//	dbgLog("%s\n", dir);
	while(*tmp)
	{
		if(*tmp == '/')
		{
			*tmp = '\0';
			DIR* d = opendir(dir);
			int ret;
			if(!d)
				ret = mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); // 755
			else
				closedir(d);
//			dbgLog("%d, %s\n", ret, dir);
			*tmp = '/';
			if(ret != EEXIST && ret)
				return ret;
		}
		tmp++;
	}
	return 0;
}

static struct fileLines empty =
{
	.next = NULL,
	.line = "",
};

/**
 * read file into a linked list of lines
 */
struct fileLines* mkFileLines(char* file)
{
	FILE* fp;
	struct fileLines* lines = NULL, *lastLn, *head = NULL;
	int c, lastC = '\n';
	int loc;
	char* tmp;

	fp = fopen(file, "r");
	if(!fp)
		return &empty;

	fseek(fp, 0, SEEK_END);
	size_t flen = ftell(fp);
	if(!flen)
		return &empty;
	fseek(fp, 0, SEEK_SET);
	
	while((c = fgetc(fp)) != -1)
	{
		if(lastC == '\n')
		{
			lastLn = lines;
			lines = calloc(1, sizeof(struct fileLines));
			if(!lines)
			{
				freeFileLines(head);
				return NULL;
			}

			if(lastLn)
			{
				lastLn->next = lines;
				lastLn->line = realloc(lastLn->line, loc + 1);
				if(!lastLn->line)
				{
					freeFileLines(head);
					return NULL;
				}
				*(lastLn->line + loc) = 0;
			}
			else
				head = lines;

			loc = 0;
		}

		if(lines->line)
			tmp = realloc(lines->line, loc + 1);
		else
			tmp = calloc(1, 1);
		if(!tmp)
		{
			freeFileLines(head);
			return NULL;
		}
		lines->line = tmp;

		*(lines->line + loc++) = c;

		lastC = c;
	}
	if(*(lines->line + (loc - 1)) != '\n')
	{
		tmp = realloc(lines->line, strlen(lines->line) + 2);
		if(!tmp)
		{
			freeFileLines(head);
			return NULL;
		}
		lines->line = tmp;
		*(lines->line + loc++) = '\n';
	}
	else
	{
		tmp = realloc(lines->line, loc + 1);
		if(!tmp)
		{
			freeFileLines(head);
			return NULL;
		}
		lines->line = tmp;
	}
	*(lines->line + loc) = '\0';

	fclose(fp);
	return head;
}

void freeFileLines(struct fileLines* lines)
{
	if(!lines) // Passed a null pointer
		return;

	free(lines->line);
	freeFileLines(lines->next);
	free(lines);

	return;
}

struct fileLines* freeSomeFileLines(struct fileLines* lines, char* stop)
{
	struct fileLines* line;

	if(!lines) // Passed a null pointer
		return NULL;

	if(strcmp(lines->line, stop))
	{
		free(lines->line);
		line = lines->next;
		free(lines);
		return freeSomeFileLines(line, stop);
	}

	return lines;
}

/**
 * returns 0 on success
 * inserts line immediately after lines
 */
int insertFileLine(struct fileLines* lines, char* new)
{
	struct fileLines* line;

	if(!lines)
		return 1;

	line = lines->next;
	lines->next = malloc(sizeof(struct fileLines));
	if(!lines->next)
	{
		return 2;
	}
	lines = lines->next;
	lines->line = calloc(strlen(new) + 1, 1);
	if(!lines->line)
	{
		lines->line = "";
		return 3;
	}
	strcpy(lines->line, new);
	lines->next = line;

	return 0;
}

/**
 * returns >=0 on success, negative on fail
 * returns input in pass, this must be free()'d by the caller
 * displays msg
 * hide=0 just reads stdin, any other value of hide uses termios to disable terminal echo
 * gets one line from stdin and  replaces the newline with a null terminator
 *
 * Maybe I should look at sudo's tgetpass.c to make this more portable, like working in a screen or in an emacs shell, but maybe later...
 */
int askStr(const char* msg, char** pass, int hide)
{
	struct termios old, new;
	size_t initlen = 100;
	*pass = malloc(100);
	if(!*pass)
		return -1;
	
	if(hide)
	{
		int ret = tcgetattr(0, &old);
		if(ret)
		{
			free(*pass);
			return -2;
		}
		
		new = old;
		new.c_lflag &= ~ECHO;
		ret = tcsetattr(0, TCSAFLUSH, &new);
		if(ret)
		{
			free(*pass);
			return -3;
		}
	}
	
	printf("%s", msg);
	fflush(stdout);

	int len = getline(pass, &initlen, stdin);
	(*pass)[len-1] = '\0';
	len--;
	
	if(hide)
	{
		tcsetattr(0, TCSAFLUSH, &old);
		printf("\n");
	}
	
	return len;
}

struct writectx
{
	char** data;
	int len;
};
static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	struct writectx* ctx = (struct writectx*)userdata;
	
	char* tmp = realloc(*ctx->data, ctx->len + size*nmemb + 1);
	if(!tmp)
		return 0;
	*ctx->data = tmp;
	
	memcpy(*ctx->data + ctx->len, ptr, size*nmemb);
	
	ctx->len += size*nmemb;
	(*ctx->data)[ctx->len] = '\0';
	
	return size*nmemb;
}
struct readctx
{
	char* data;
	int off;
	int size;
};
static size_t curl_read_cb(char* buffer, size_t size, size_t nitems, void* userdata)
{
	struct readctx* ctx = (struct readctx*)userdata;
	
	size_t sz = min(size*nitems, ctx->size - ctx->off);
	
	memcpy(buffer, ctx->data + ctx->off, sz);
	
	ctx->off += sz;
	
	return 0;
}
/**
 * returns positive length on success
 * outputs text reply from server in resp, this must be free()'d by the caller
 * req can be NULL for an empty request
 * reqlen should be the length of the string req, if -1, strlen() will be called
 * user should call curl_global_init before calling this
 */
int doCurl(char** resp, const char* url, const char* req, int reqlen, enum HTTPreqType type)
{
	CURL* curl = curl_easy_init();
	if(!curl)
	{
		return -1;
	}
	
	CURLcode res;
	if(curl_easy_setopt(curl, CURLOPT_URL, url))
	{
		curl_easy_cleanup(curl);
		return -2;
	}
	
	if(type == HTTP_POST)
	{
		if(curl_easy_setopt(curl, CURLOPT_POST, 1L))
		{
			curl_easy_cleanup(curl);
			return -3;
		}
	}
	if(type == HTTP_GET)
	{
		if(curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L))
		{
			curl_easy_cleanup(curl);
			return -4;
		}
	}
	
	if(req && reqlen < 0)
		reqlen = strlen(req);
	if(req && reqlen)
	{
		if(curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb))
		{
			curl_easy_cleanup(curl);
			return -5;
		}
		if(curl_easy_setopt(curl, CURLOPT_READDATA, req))
		{
			curl_easy_cleanup(curl);
			return -6;
		}
		if(curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L))
		{
			curl_easy_cleanup(curl);
			return -7;
		}
	}
	*resp = NULL;
	if(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb))
	{
		curl_easy_cleanup(curl);
		return -8;
	}
	struct writectx* wctx = malloc(sizeof(struct writectx));
	if(!wctx)
	{
		curl_easy_cleanup(curl);
		return -9;
	}
	wctx->data = resp;
	wctx->len = 0;
	if(curl_easy_setopt(curl, CURLOPT_WRITEDATA, wctx))
	{
		curl_easy_cleanup(curl);
		return -10;
	}
	
	res = curl_easy_perform(curl);
	if(res != 0)
	{
		curl_easy_cleanup(curl);
		free(*resp);
		*resp = NULL;
		return -11;
	}
	
	curl_easy_cleanup(curl);
	
	return wctx->len;
}

///**
// * returns 0 on success
// * when successful, caller must free *out
// * finds ${whatever} is strings and replaces with the value of the environment variable called whatever
// * for a literal "$" to be preserved, you must enter as "${}"
// */
//int envexpand(const char* in, char** out)
//{
//	int addedLen = 0;
//	int removedLen = 0;
//	int initLen = strlen(in);
//	char tmp[initLen];
//
//	// State machine to parse the input
//	int varFound = 0;
//	int varLen = 0;
//	for(int i = 0; i < initLen; i++)
//	{
//		if(varFound && in[i] == '}')
//		{
//			varFound = 0;
//			tmp[varLen] = '\0';
//		}
//		if(varFound)
//		{
//			tmp[varLen++] = in[i];
//		}
//		if(!varFound && in[i] == '$')
//		{
//			varFound = 1;
//			varLen = 0;
//		}
//	}
//
//	return 0;
//}

void tolcase(char* str, int len)
{
	for(int i = 0; i < len; i++)
	{
		if(*str >= 'A' && *str <= 'Z')
		{
			*str |= 0x20; // ASCII shift bit
		}
		str++;
	}
	return;
}

/**
 * str "bang' chr nul
 * acts like chrstrnull, but finds the first char that isn't c
 */
char* strbchrnul(char* str, int c)
{
	while(*str && *str == c)
		str++;
	return str;
}

void dumpBN(char* msg, BIGNUM* bn)
{
	char tmp[10000];
	
	BN_bn2bin(bn, tmp);
	
	hexdump(msg, tmp, BN_num_bytes(bn));
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

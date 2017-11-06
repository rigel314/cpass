/*
 * util.h
 *
 *  Created on: Nov 24, 2016
 *      Author: cody
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <stdio.h>
#include <openssl/crypto.h>

extern int base64expansion(int len);
extern int base64contraction(int len);

#define showErrs() showErrQueue(__FILE__, __LINE__)

// Macro for iterating through any linked list.
#define LLforeach(type, ptr, list) for(type ptr = list; ptr != NULL; ptr = ptr->next)

#define voidStatment (void) (2+2)

#define dbgLog(str, ...) printf("%s:%d %s(): " str, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

// Usual min and max macros.
#define min(x, y) ((x)<(y)?(x):(y))
#define max(x, y) ((x)>(y)?(x):(y))

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

struct fileLines
{
	struct fileLines* next;
	char* line;
};

enum HTTPreqType { HTTP_GET, HTTP_POST };

int base64contraction(int len);
int base64expansion(int len);
int base64enc(char* b64, int len, const char* pt);
int base64dec(char* pt, int len, const char* b64);
void showErrQueue(char* file, int line);
int strnrcmp(const char* s1, const char* s2, int len);
int mkdirp(char* dir);
struct fileLines* mkFileLines(char* file);
void freeFileLines(struct fileLines* lines);
struct fileLines* freeSomeFileLines(struct fileLines* lines, char* stop);
int insertFileLine(struct fileLines* lines, char* new);
int askStr(const char* msg, char** pass, int hide);
int doCurl(char** resp, const char* url, const char* req, int reqlen, enum HTTPreqType type);
void tolcase(char* str, int len);
char* strbchrnul(char* str, int c);
void dumpBN(char* msg, BIGNUM* bn);
void hexdump(const char* message, const char* data, int len);
void decdump(const char* message, const char* data, int len);

#ifdef TESTS
int testIntcmp(char* msg, int truth, int computed);
int testMemcmp(char* msg, char* truth, char* computed, int len);
int testBase64();
int testStrnrcmp();
#endif

#endif /* SRC_UTIL_H_ */

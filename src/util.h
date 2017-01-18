/*
 * util.h
 *
 *  Created on: Nov 24, 2016
 *      Author: cody
 */

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

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

int base64contraction(int len);
int base64expansion(int len);
int base64enc(char* b64, int len, const char* pt);
int base64dec(char* pt, int len, const char* b64);
void showErrQueue(char* file, int line);
int strnrcmp(const char* s1, const char* s2, int len);
void hexdump(const char* message, const char* data, int len);
void decdump(const char* message, const char* data, int len);

#ifdef TESTS
int testIntcmp(char* msg, int truth, int computed);
int testMemcmp(char* msg, char* truth, char* computed, int len);
int testBase64();
int testStrnrcmp();
#endif

#endif /* SRC_UTIL_H_ */

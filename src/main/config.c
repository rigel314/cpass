/*
 * config.c
 *
 *  Created on: Jun 1, 2017
 *      Author: cody
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wordexp.h>
#include "util.h"

char* defaultConfigFile = "~/.cpass/cpassConfig";

/**
 * looks for an entry with a matching name
 * returns 0 on success
 * returns string of value in val
 * caller needs to free *val
 * when there are multiple identical names, you get the value of the last one.
 */
int getConfigStr(const char* name, char** val, const char* file)
{
	if(!file)
		file = defaultConfigFile;

	*val = NULL;

	wordexp_t p;
	int ret = wordexp(file, &p, WRDE_NOCMD);
	if(ret)
	{
		wordfree(&p);
		return -ret;
	}
	if(p.we_wordc != 1)
	{
		wordfree(&p);
		return 2;
	}
	
	FILE* fp = fopen(p.we_wordv[0], "r");
	if(!fp)
	{
		wordfree(&p);
		return 1;
	}
	
	char* line = NULL;
	int nameLen = strlen(name);
	int len = 0;
	size_t size = 0;
	while((len = getline(&line, &size, fp)) != -1)
	{
		char* eql = strchr(line, '=');
		if(eql)
		{
			int thisNameLen = eql - line;
			if(nameLen == thisNameLen && !strncmp(line, name, thisNameLen))
			{
				int vallen = len - thisNameLen - 1; // includes NULL
				if(*val)
					free(*val);
				*val = malloc(vallen);
				if(!*val)
				{
					free(line);
					fclose(fp);
					return 2;
				}
				memcpy(*val, eql+1, vallen);
				(*val)[vallen-1] = '\0';
			}
		}
		
		free(line);
		line = NULL;
		size = 0;
	}
	
	fclose(fp);
	
	wordfree(&p);
	return 0;
}

/**
 * returns 0 on success
 * creates or edits the value of every entry with matching name
 */
int setConfigStr(const char* name, const char* val, const char* file)
{
	char* tmp;
	
	if(!file)
		file = defaultConfigFile;
	
	wordexp_t p;
	int ret = wordexp(file, &p, WRDE_NOCMD);
	if(ret)
	{
		wordfree(&p);
		return -ret;
	}
	if(p.we_wordc != 1)
	{
		wordfree(&p);
		return 1;
	}
	
	ret = mkdirp(p.we_wordv[0]);
	if(ret)
	{
		wordfree(&p);
		return 2;
	}
	
	struct fileLines* lines = mkFileLines(p.we_wordv[0]);
	if(!lines)
	{
		wordfree(&p);
		return 3;
	}
	
	int nameLen = strlen(name);
	int valLen = strlen(val) + 1; // NULL
	int found = 0;
	struct fileLines* end = lines;
	LLforeach(struct fileLines*, ptr, lines)
	{
		char* line = ptr->line;
		int len = strlen(line);
		char* eql = strchr(line, '=');
		if(eql)
		{
			int thisNameLen = eql - line;
			if(nameLen == thisNameLen && !strncmp(line, name, thisNameLen))
			{
//				int thisValLen = len - thisNameLen - 1; // includes NULL and \n
				if(len + 1 < valLen + nameLen + 2) // '=' and \n
				{
					tmp = realloc(line, valLen+nameLen+2); // '=' and \n
					if(!tmp)
					{
						freeFileLines(lines);
						wordfree(&p);
						return 4;
					}
					line = tmp;
				}
				memcpy(eql+1, val, valLen);
				strcat(eql, "\n");
				found = 1;
			}
		}
		end = ptr;
	}
	if(!found)
	{
		tmp = malloc(nameLen + valLen + 2); // '=' and \n
		if(!tmp)
		{
			freeFileLines(lines);
			wordfree(&p);
			return 5;
		}
		strcpy(tmp, name);
		strcat(tmp, "=");
		strcat(tmp, val);
		strcat(tmp, "\n");
		insertFileLine(end, tmp);
		free(tmp);
	}
	
	FILE* fp = fopen(p.we_wordv[0], "w");
	if(!fp)
	{
		freeFileLines(lines);
		wordfree(&p);
		return 6;
	}
	
	LLforeach(struct fileLines*, ptr, lines)
	{
		fwrite(ptr->line, 1, strlen(ptr->line), fp);
	}
	
	fclose(fp);
	
	wordfree(&p);
	return 0;
}

/*
 * gui.c
 *
 *  Created on: Apr 22, 2017
 *      Author: cody
 */

#include "gui.h"

enum GUImode gmode = GFX_DEFAULT;

int initializeGUI()
{
	GUICALL(initializeGUI())
	return 0;
}

int askPass(const char* msg, char** pass)
{
	GUICALL(askPass(msg, pass))
	return 0;
}

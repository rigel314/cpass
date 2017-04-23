/*
 * gui.h
 *
 *  Created on: Apr 22, 2017
 *      Author: cody
 */

#ifndef SRC_GUI_GUI_H_
#define SRC_GUI_GUI_H_

#include "util.h"

#define CONCAT(x,y) x ## y
#define CONCATPRE(x,y) CONCAT(x,y)
#define GUI(ret,x) ADDGTKPREFIX(ret,x) \
ADDCURSESPREFIX(ret,x) \
ADDKDEPREFIX(ret,x)

#define GUICALL(x) switch(gmode) \
{ \
	case GM_GTK: \
		ADDGTKPREFIX(,x); \
		break; \
	case GM_CURSES: \
		ADDGTKPREFIX(,x); \
		break; \
	case GM_KDE: \
		ADDGTKPREFIX(,x); \
		break; \
	default: \
		dbgLog("GUI function called when no GUI selected.\n"); \
		break; \
}

// Set function prefixes based on compiler defines
#ifdef GFX_GTK
	#define gtkPREFIX gtk_
	#define ADDGTKPREFIX(ret,x) ret CONCATPRE(gtkPREFIX, x)
#else
	#define ADDGTKPREFIX(ret,x)
#endif
#ifdef GFX_CURSES
	#define cursPREFIX curses_
	#define ADDCURSESPREFIX(ret,x) ret CONCATPRE(cursPREFIX, x)
#else
	#define ADDCURSESPREFIX(ret,x)
#endif
#ifdef GFX_KDE
	#define kdePREFIX curses_
	#define ADDKDEPREFIX(ret,x) ret CONCATPRE(kdePREFIX, x)
#else
	#define ADDKDEPREFIX(ret,x)
#endif

// Set default value when one GUI was chosen.
#if defined(GFX_GTK) && !defined(GFX_KDE) && !defined(GFX_CURSES)
	#define GFX_DEFAULT GM_GTK
#endif

#if !defined(GFX_GTK) && defined(GFX_KDE) && !defined(GFX_CURSES)
	#define GFX_DEFAULT GM_KDE
#endif

#if !defined(GFX_GTK) && !defined(GFX_KDE) && defined(GFX_CURSES)
	#define GFX_DEFAULT GM_CURSES
#endif

#if !defined(GFX_GTK) && !defined(GFX_KDE) && !defined(GFX_CURSES)
	#define GFX_DEFAULT GM_NONE
#endif

// Prototype standard functions in gui agnostic way.
GUI(int, initializeGUI();)
GUI(int, askPass(const char* msg, char** out);)

// wrapper functions
int initializeGUI();
int askPass(const char* msg, char** out);

enum GUImode { GM_NONE, GM_CURSES, GM_GTK, GM_KDE };

extern enum GUImode gmode;

#endif /* SRC_GUI_GUI_H_ */

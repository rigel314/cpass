/*
 * config.h
 *
 *  Created on: Jun 1, 2017
 *      Author: cody
 */

#ifndef SRC_MAIN_CONFIG_H_
#define SRC_MAIN_CONFIG_H_

/**
 * About config files:
 *	The config file is stored at ~/.cpass/cpassConfig
 *	This file contains name/value pairs.
 *	At this time, the format is very particular.
 *		1. Each name must be at the beginning of a line.
 *		2. The name is immediately followed by an equal sign ('=') with no space.
 *		3. The equal sign is immediately followed by an ASCII representation of the value with no space.
 *		4. Binary data should be base64url encoded.
 *		5. Values must not contain newlines, if they contain newlines, they must be treated as binary data and base64url encoded.
 *		6. The file must end in a new line.
 *		e.g.
 *			clientUUID=12345678901234567890123456
 *			encAccKey=56ao4i6o4ia-6_oUaA
 *			someIntVal=5
 *			email=aoeu@example.com
 *			
 */

// standard config variable names
#define CNF_VAR_clientUUID "clientUUID"
#define CNF_VAR_encAccKey "encAccKey"
#define CNF_VAR_email "email"
#define CNF_VAR_url "url"

#define CNF_DEF_FILE NULL

int getConfigStr(const char* name, char** val, const char* file);
int setConfigStr(const char* name, const char* val, const char* file);

#endif /* SRC_MAIN_CONFIG_H_ */

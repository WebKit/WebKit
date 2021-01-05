/*
 * Copyright 2012, Alexandre Deckner, alexandre.deckner@uzzl.com.
 * Distributed under the terms of the MIT License.
 */
#ifndef WEBKIT_INFO_H_
#define WEBKIT_INFO_H_

#include <String.h>


class __attribute__ ((visibility ("default"))) WebKitInfo {
public:
	static	BString				HaikuWebKitVersion();
	static	BString				WebKitVersion();
	static	int					WebKitMajorVersion();
	static	int					WebKitMinorVersion();
	static	int					WebKitTinyVersion();
};


#endif	// WEBKIT_INFO_H_

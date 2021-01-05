/*
 * Copyright 2012, Alexandre Deckner, alexandre.deckner@uzzl.com.
 * Distributed under the terms of the MIT License.
 */


#include "WebKitInfo.h"

#include "WebKitVersion.h"

#include <String.h>


/*static*/ BString
WebKitInfo::HaikuWebKitVersion()
{
	return HAIKU_WEBKIT_VERSION;
}


/*static*/ BString
WebKitInfo::WebKitVersion()
{
	return BString() << WEBKIT_MAJOR_VERSION << "." << WEBKIT_MINOR_VERSION << "." << WEBKIT_TINY_VERSION;
}


/*static*/ int
WebKitInfo::WebKitMajorVersion()
{
    return WEBKIT_MAJOR_VERSION;
}


/*static*/ int
WebKitInfo::WebKitMinorVersion()
{
    return WEBKIT_MINOR_VERSION;
}


/*static*/ int
WebKitInfo::WebKitTinyVersion()
{
    return WEBKIT_TINY_VERSION;
}

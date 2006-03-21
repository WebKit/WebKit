/*
 *  WebNetscapeDeprecatedFunctions.c
 *  WebKit
 *
 *  Created by Tim Omernick on 3/21/06.
 *  Copyright 2006 Apple Computer, Inc. All rights reserved.
 *
 */

#include "WebNetscapeDeprecatedFunctions.h"

#if !__LP64__

OSErr WebGetDiskFragment(const FSSpec *fileSpec, UInt32 offset, UInt32 length, ConstStr63Param fragName, CFragLoadOptions options, CFragConnectionID *connID, Ptr *mainAddr, Str255 errMessage)
{
    return GetDiskFragment(fileSpec, offset, length, fragName, options, connID, mainAddr, errMessage);
}

OSErr WebCloseConnection(CFragConnectionID *connID)
{
    return CloseConnection(connID);
}

SInt16 WebLMGetCurApRefNum(void)
{
    return LMGetCurApRefNum();
}

extern void WebLMSetCurApRefNum(SInt16 value)
{
    LMSetCurApRefNum(value);
}

#endif /* !__LP64__ */

/*
 *  WebNetscapeDeprecatedFunctions.h
 *  WebKit
 *
 *  Created by Tim Omernick on 3/21/06.
 *  Copyright 2006 Apple Computer, Inc. All rights reserved.
 *
 */

#if !__LP64__

#import <CoreServices/CoreServices.h>

extern OSErr WebGetDiskFragment(const FSSpec *fileSpec, UInt32 offset, UInt32 length, ConstStr63Param fragName, CFragLoadOptions options, CFragConnectionID *connID, Ptr *mainAddr, Str255 errMessage);
extern OSErr WebCloseConnection(CFragConnectionID *connID);
extern SInt16 WebLMGetCurApRefNum(void);
extern void WebLMSetCurApRefNum(SInt16 value);

#endif /* !__LP64__ */

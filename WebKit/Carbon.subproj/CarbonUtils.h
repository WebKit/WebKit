/*
    CarbonUtils.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/
#ifndef __HIWEBCARBONUTILS__
#define __HIWEBCARBONUTILS__

#import <Carbon/Carbon.h>
#import <WebKit/WebKit.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void 		InitWebKitForCarbon( void );
extern CGImageRef 	ConvertNSImageToCGImageRef( NSImage* inImage );

#ifdef __cplusplus
}
#endif

#endif // __HIWEBCARBONUTILS__
/*
 *  CarbonUtils.h
 *  WebKit
 *
 *  Created by Ed Voas on Mon Feb 17 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
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
/*
    CarbonUtils.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/

#ifndef __HIWEBCARBONUTILS__
#define __HIWEBCARBONUTILS__

#ifdef __OBJC__
#import <ApplicationServices/ApplicationServices.h>
@class NSImage;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void
WebInitForCarbon(void);

#ifdef __OBJC__

extern CGImageRef
WebConvertNSImageToCGImageRef(NSImage * inImage);

#endif

#ifdef __cplusplus
}
#endif

#endif // __HIWEBCARBONUTILS__

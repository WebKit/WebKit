/*
    WebNSObjectExtras.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

static inline id WebMakeCollectable(CFTypeRef obj)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3
    return (id)obj;
#else
    return (id)CFMakeCollectable(obj);
#endif
}

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3

@interface NSObject (WebExtras)
- (void)finalize;
@end

#endif

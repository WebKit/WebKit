/*
    WebNSObjectExtras.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

static inline id WebNSRetainCFRelease(CFTypeRef obj)
{
#if BUILDING_ON_PANTHER
    return (id)obj;
#else
    return (id)CFMakeCollectable(obj);
#endif
}

#if BUILDING_ON_PANTHER

@interface NSObject (WebExtras)
- (void)finalize;
@end

#endif

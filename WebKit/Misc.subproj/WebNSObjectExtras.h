/*
    WebNSObjectExtras.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

// Use WebCFAutorelease to return an object made by a CoreFoundation
// "create" or "copy" function as an autoreleased and garbage collected
// object. CF objects need to be "made collectable" for autorelease to work
// properly under GC.

static inline id WebCFAutorelease(CFTypeRef obj)
{
#if !BUILDING_ON_PANTHER
    if (obj) CFMakeCollectable(obj);
#endif
    [(id)obj autorelease];
    return (id)obj;
}

#if BUILDING_ON_PANTHER

@interface NSObject (WebExtras)
- (void)finalize;
@end

#endif

/*
        WebFormDelegatePrivate.h
        Copyright 2003, Apple Computer, Inc.
 */

#import "WebFormDelegate.h"

@interface WebFormDelegate (WebPrivate)
+ (WebFormDelegate *)_sharedWebFormDelegate;
@end


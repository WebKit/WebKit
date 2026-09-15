/* CoreImage - CIFilterConstructor.h
 
 Copyright (c) 2014 Apple, Inc.
 All rights reserved. */

#ifndef CIFILTERCONSTRUCTOR_H
#define CIFILTERCONSTRUCTOR_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>

@class CIFilter;

// Used by +[CIFilter registerFilterName:constructor:classAttributes:]

NS_ASSUME_NONNULL_BEGIN

@protocol CIFilterConstructor
- (nullable CIFilter *)filterWithName:(NSString *)name;
@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIFILTERCONSTRUCTOR_H */

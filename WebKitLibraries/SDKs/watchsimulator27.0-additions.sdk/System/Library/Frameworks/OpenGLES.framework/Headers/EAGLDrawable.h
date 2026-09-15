/*
    Copyright:  (c) 2006-2013 Apple Inc. All rights reserved.
*/

#ifndef _EAGL_DRAWABLE_H_
#define _EAGL_DRAWABLE_H_

#include <OpenGLES/EAGL.h>
#include <OpenGLES/OpenGLESAvailability.h>

NS_ASSUME_NONNULL_BEGIN

/************************************************************************/
/* Keys for EAGLDrawable drawableProperties dictionary                  */
/*                                                                      */
/* kEAGLDrawablePropertyRetainedBacking:                                */
/*  Type: NSNumber (boolean)                                            */
/*  Legal Values: True/False                                            */
/*  Default Value: False                                                */
/*  Description: True if EAGLDrawable contents are retained after a     */
/*               call to presentRenderbuffer.  False, if they are not   */
/*                                                                      */
/* kEAGLDrawablePropertyColorFormat:                                    */
/*  Type: NSString                                                      */
/*  Legal Values: kEAGLColorFormat*                                     */
/*  Default Value: kEAGLColorFormatRGBA8                                */
/*  Description: Format of pixels in renderbuffer                       */
/************************************************************************/
EAGL_EXTERN NSString * const kEAGLDrawablePropertyRetainedBacking OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0));
EAGL_EXTERN NSString * const kEAGLDrawablePropertyColorFormat OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0));

/************************************************************************/
/* Values for kEAGLDrawablePropertyColorFormat key                      */
/************************************************************************/
EAGL_EXTERN NSString * const kEAGLColorFormatRGBA8 OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0));
EAGL_EXTERN NSString * const kEAGLColorFormatRGB565 OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0));
EAGL_EXTERN NSString * const kEAGLColorFormatSRGBA8 OPENGLES_DEPRECATED(ios(7.0, 12.0), tvos(9.0, 12.0));

/************************************************************************/
/* EAGLDrawable Interface                                               */
/************************************************************************/

OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0))
@protocol EAGLDrawable 

/* Contains keys from kEAGLDrawableProperty* above */
@property(nullable, copy) NSDictionary<NSString*, id>* drawableProperties;

@end

/* Extends EAGLContext interface */
OPENGLES_DEPRECATED(ios(2.0, 12.0), tvos(9.0, 12.0))
@interface EAGLContext (EAGLContextDrawableAdditions)

/* Attaches an EAGLDrawable as storage for the OpenGL ES renderbuffer object bound to <target> */
- (BOOL)renderbufferStorage:(NSUInteger)target fromDrawable:(nullable id<EAGLDrawable>)drawable;

/* Request the native window system display the OpenGL ES renderbuffer bound to <target> */
- (BOOL)presentRenderbuffer:(NSUInteger)target;

/* Request the native window system display the OpenGL ES renderbuffer bound to <target> at specified time */
- (BOOL)presentRenderbuffer:(NSUInteger)target atTime:(CFTimeInterval)presentationTime;

/* Request the native window system display the OpenGL ES renderbuffer bound to <target> after the previous frame is presented for at least duration time */
- (BOOL)presentRenderbuffer:(NSUInteger)target afterMinimumDuration:(CFTimeInterval)duration;

@end /* EAGLDrawable protocol */

NS_ASSUME_NONNULL_END

#endif /* _EAGL_DRAWABLE_H_ */


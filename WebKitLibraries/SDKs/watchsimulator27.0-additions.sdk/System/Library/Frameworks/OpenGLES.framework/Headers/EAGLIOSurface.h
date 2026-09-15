/*
    Copyright:  (c) 2017 Apple Inc. All rights reserved.
*/

#ifndef _EAGL_IOSURFACE_H_
#define _EAGL_IOSURFACE_H_

#include <OpenGLES/EAGL.h>


#if defined(__IPHONE_11_0)

#include <IOSurface/IOSurfaceRef.h>

NS_ASSUME_NONNULL_BEGIN

OPENGLES_DEPRECATED(ios(11.0, 12.0), tvos(11.0, 12.0))
@interface EAGLContext(IOSurface)
- (BOOL)texImageIOSurface:(IOSurfaceRef)ioSurface target:(NSUInteger)target internalFormat:(NSUInteger)internalFormat width:(uint32_t)width height:(uint32_t)height format:(NSUInteger)format type:(NSUInteger)type plane:(uint32_t)plane NS_AVAILABLE_IOS(11_0);
@end

NS_ASSUME_NONNULL_END

#endif


#endif /* _EAGL_IOSURFACE_H_ */


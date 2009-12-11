/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 */

#ifndef QuartzCoreInterface_h
#define QuartzCoreInterface_h

#ifdef QUARTZCOREINTERFACE_EXPORTS
#define QUARTZCOREINTERFACE_API __declspec(dllexport)
#else
#define QUARTZCOREINTERFACE_API __declspec(dllimport)
#endif

// Interface to give access to QuartzCore data symbols.
enum WKQCStringRefType { wkqckCACFLayer, wkqckCACFTransformLayer, wkqckCACFFilterLinear, wkqckCACFFilterNearest,
                         wkqckCACFFilterTrilinear, wkqckCACFFilterLanczos, wkqckCACFGravityCenter, wkqckCACFGravityTop, 
                         wkqckCACFGravityBottom, wkqckCACFGravityLeft, wkqckCACFGravityRight, wkqckCACFGravityTopLeft, 
                         wkqckCACFGravityTopRight, wkqckCACFGravityBottomLeft, wkqckCACFGravityBottomRight, 
                         wkqckCACFGravityResize, wkqckCACFGravityResizeAspect, wkqckCACFGravityResizeAspectFill };

enum WKQCCARenderOGLCallbacksType { wkqckCARenderDX9Callbacks };

typedef const struct __CFString * CFStringRef;
typedef struct _CARenderOGLCallbacks CARenderOGLCallbacks;
typedef struct CATransform3D CATransform3D;

extern "C" {
QUARTZCOREINTERFACE_API CFStringRef wkqcCFStringRef(WKQCStringRefType);
QUARTZCOREINTERFACE_API const CARenderOGLCallbacks* wkqcCARenderOGLCallbacks(WKQCCARenderOGLCallbacksType);
QUARTZCOREINTERFACE_API const CATransform3D& wkqcCATransform3DIdentity();
}

#endif // QuartzCoreInterface_h

/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "WebKitQuartzCoreAdditionsBase.h"
#include <QuartzCore/CoreAnimationCF.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WKQCA_DEFINED_WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <d3d9.h>
#include <windows.h>

#ifdef WKQCA_DEFINED_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the type identifier of all WKCACFView instances. */
WKQCA_EXPORT CFTypeID WKCACFViewGetTypeID(void);

enum WKCACFViewDrawingDestination {
    /* Drawing is done directly into the window passed to WKCACFViewUpdate. Drawing occurs
     * automatically and asynchronously after WKCACFViewFlushContext is called. Synchronous drawing
     * can be achieved by calling WKCACFViewDraw. */
    kWKCACFViewDrawingDestinationWindow = 0,

    /* Drawing is done to an image in system memory. Drawing is manually driven by the caller via
     * the WKCACFViewCopyDrawnImage API. The window passed to WKCACFViewUpdate is still used by
     * Direct3D for message processing, but is not drawn into. */
    kWKCACFViewDrawingDestinationImage,
};
typedef enum WKCACFViewDrawingDestination WKCACFViewDrawingDestination;

/* Creates a new view object. */
WKQCA_EXPORT WKCACFViewRef WKCACFViewCreate(WKCACFViewDrawingDestination);

/* Sets the root layer being displayed by the view. */
WKQCA_EXPORT void WKCACFViewSetLayer(WKCACFViewRef, CACFLayerRef);

/* Sets the window associated with the view. The passed-in bounds
 * should match the window's client rect. */
WKQCA_EXPORT void WKCACFViewUpdate(WKCACFViewRef, HWND window, const CGRect* bounds);

/* Commit all changes made to view and the layer tree it references to
 * the render tree (i.e. to the screen). This function must be called
 * after modifying any layer properties or adding any animations to
 * have their effect be seen. */
WKQCA_EXPORT void WKCACFViewFlushContext(WKCACFViewRef);

/* Invalidates a region of the view, i.e. causes it to be redrawn the next
 * time the view redraws anything. */
WKQCA_EXPORT void WKCACFViewInvalidateRects(WKCACFViewRef, const CGRect rects[], size_t count);

/* Returns true if it is possible for this view to draw into the window
 * at this time. This can return false if, e.g., the system does not
 * meet CoreAnimation's hardware requirements. */
WKQCA_EXPORT bool WKCACFViewCanDraw(WKCACFViewRef);

/* Renders the current region needing updating into the view's window. May only be called when the
 * view was created with kWKCACFViewDrawingDestinationWindow. */
WKQCA_EXPORT void WKCACFViewDraw(WKCACFViewRef);

/* Renders the current region needing updating and returns it as an image. imageOrigin specifies
 * the location within the view to which the image corresponds, relative to the bottom-left. May
 * only be called when the view was created with kWKCACFViewDrawingDestinationImage. */
WKQCA_EXPORT WKCACFImageRef WKCACFViewCopyDrawnImage(WKCACFViewRef, CGPoint* imageOrigin, CFTimeInterval* nextDrawTime);

/* Renders the entire view into the device context. */
WKQCA_EXPORT void WKCACFViewDrawIntoDC(WKCACFViewRef, HDC);

/* Sets a function to be called whenever the view's root layer has changed and needs to be redrawn. */
typedef void (*WKCACFViewContextDidChangeCallback)(WKCACFViewRef view, void* info);
WKQCA_EXPORT void WKCACFViewSetContextDidChangeCallback(WKCACFViewRef, WKCACFViewContextDidChangeCallback, void* info);

/* Returns the default beginTime of animations added as part of the
 * previous transaction (i.e. the previous call to WKCACFViewFlushContext). */
WKQCA_EXPORT CFTimeInterval WKCACFViewGetLastCommitTime(WKCACFViewRef);

/* Allows for an arbitrary pointer to be associated with the view's context. */
WKQCA_EXPORT void WKCACFViewSetContextUserData(WKCACFViewRef, void* userData);

/* Set whether the view should invert colors when rendering */
WKQCA_EXPORT void WKCACFViewSetShouldInvertColors(WKCACFViewRef, bool shouldInvertColors);

/* Returns a pointer to the D3D device. The caller is responsible for retaining the object if 
 * it will be used after the function returns. */
WKQCA_EXPORT IDirect3DDevice9* WKCACFViewGetD3DDevice9(WKCACFViewRef);

#ifdef __cplusplus
}
#endif

/*
 * Copyright (C) 2009, 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CanvasBase.h"
#include "GraphicsLayer.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CanvasPattern;
class DestinationColorSpace;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class CSSStyleImageValue;
class WebGLObject;
enum class PixelFormat : uint8_t;

class CanvasRenderingContext : public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CanvasRenderingContext);
    WTF_MAKE_ISO_ALLOCATED(CanvasRenderingContext);
public:
    virtual ~CanvasRenderingContext();

    static HashSet<CanvasRenderingContext*>& instances() WTF_REQUIRES_LOCK(instancesLock());
    static Lock& instancesLock() WTF_RETURNS_LOCK(s_instancesLock);

    void ref();
    WEBCORE_EXPORT void deref();

    CanvasBase& canvasBase() const { return m_canvas; }

    virtual bool is2d() const { return false; }
    virtual bool isWebGL1() const { return false; }
    virtual bool isWebGL2() const { return false; }
    bool isWebGL() const { return isWebGL1() || isWebGL2(); }
    virtual bool isGPUBased() const { return false; }
    virtual bool isAccelerated() const { return false; }
    virtual bool isBitmapRenderer() const { return false; }
    virtual bool isPlaceholder() const { return false; }
    virtual bool isOffscreen2d() const { return false; }
    virtual bool isPaint() const { return false; }

    virtual void clearAccumulatedDirtyRect() { }

    // Called before paintRenderingResultsToCanvas if paintRenderingResultsToCanvas is
    // used for compositing purposes.
    virtual void prepareForDisplayWithPaint() { }
    virtual void paintRenderingResultsToCanvas() { }
    virtual PlatformLayer* platformLayer() const { return 0; }

    bool hasActiveInspectorCanvasCallTracer() const { return m_hasActiveInspectorCanvasCallTracer; }
    void setHasActiveInspectorCanvasCallTracer(bool hasActiveInspectorCanvasCallTracer) { m_hasActiveInspectorCanvasCallTracer = hasActiveInspectorCanvasCallTracer; }

    virtual bool compositingResultsNeedUpdating() const { return false; }
    virtual bool needsPreparationForDisplay() const { return false; }
    virtual void prepareForDisplay() { }

    virtual PixelFormat pixelFormat() const;
    virtual DestinationColorSpace colorSpace() const;

protected:
    explicit CanvasRenderingContext(CanvasBase&);
    bool wouldTaintOrigin(const CanvasPattern*);
    bool wouldTaintOrigin(const CanvasBase*);
    bool wouldTaintOrigin(const HTMLImageElement*);
    bool wouldTaintOrigin(const HTMLVideoElement*);
    bool wouldTaintOrigin(const ImageBitmap*);
    bool wouldTaintOrigin(const URL&);

    template<class T> void checkOrigin(const T* arg)
    {
        if (wouldTaintOrigin(arg))
            m_canvas.setOriginTainted();
    }
    void checkOrigin(const URL&);
    void checkOrigin(const CSSStyleImageValue&);

    bool m_hasActiveInspectorCanvasCallTracer { false };

private:
    static Lock s_instancesLock;

    CanvasBase& m_canvas;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::CanvasRenderingContext& context) { return context.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

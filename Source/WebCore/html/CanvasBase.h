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

#include "IntSize.h"
#include <wtf/HashSet.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class AffineTransform;
class CanvasBase;
class CanvasRenderingContext;
class Element;
class GraphicsContext;
class GraphicsContextStateSaver;
class Image;
class ImageBuffer;
class FloatRect;
class ScriptExecutionContext;
class SecurityOrigin;
class WebCoreOpaqueRoot;

class CanvasObserver : public CanMakeWeakPtr<CanvasObserver> {
public:
    virtual ~CanvasObserver() = default;

    virtual bool isStyleCanvasImage() const { return false; }

    virtual void canvasChanged(CanvasBase&, const std::optional<FloatRect>& changedRect) = 0;
    virtual void canvasResized(CanvasBase&) = 0;
    virtual void canvasDestroyed(CanvasBase&) = 0;
};

class CanvasDisplayBufferObserver : public CanMakeWeakPtr<CanvasDisplayBufferObserver> {
public:
    virtual ~CanvasDisplayBufferObserver() = default;

    virtual void canvasDisplayBufferPrepared(CanvasBase&) = 0;
};

class CanvasBase {
public:
    virtual ~CanvasBase();

    virtual void refCanvasBase() = 0;
    virtual void derefCanvasBase() = 0;

    virtual bool isHTMLCanvasElement() const { return false; }
    virtual bool isOffscreenCanvas() const { return false; }
    virtual bool isCustomPaintCanvas() const { return false; }

    virtual unsigned width() const { return m_size.width(); }
    virtual unsigned height() const { return m_size.height(); }
    const IntSize& size() const { return m_size; }

    ImageBuffer* buffer() const;

    virtual void setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&&) { }

    virtual AffineTransform baseTransform() const;

    void makeRenderingResultsAvailable();

    size_t memoryCost() const;
    size_t externalMemoryCost() const;

    void setOriginClean() { m_originClean = true; }
    void setOriginTainted() { m_originClean = false; }
    bool originClean() const { return m_originClean; }

    virtual SecurityOrigin* securityOrigin() const { return nullptr; }
    ScriptExecutionContext* scriptExecutionContext() const { return canvasBaseScriptExecutionContext();  }

    virtual CanvasRenderingContext* renderingContext() const = 0;

    void addObserver(CanvasObserver&);
    void removeObserver(CanvasObserver&);
    void notifyObserversCanvasChanged(const std::optional<FloatRect>&);
    void notifyObserversCanvasResized();
    void notifyObserversCanvasDestroyed(); // Must be called in destruction before clearing m_context.
    void addDisplayBufferObserver(CanvasDisplayBufferObserver&);
    void removeDisplayBufferObserver(CanvasDisplayBufferObserver&);
    void notifyObserversCanvasDisplayBufferPrepared();
    bool hasDisplayBufferObservers() const { return !m_displayBufferObservers.computesEmpty(); }

    HashSet<Element*> cssCanvasClients() const;

    virtual GraphicsContext* drawingContext() const;
    virtual GraphicsContext* existingDrawingContext() const;

    virtual void didDraw(const std::optional<FloatRect>&) = 0;

    virtual Image* copiedImage() const = 0;
    virtual void clearCopiedImage() const = 0;

    bool hasActiveInspectorCanvasCallTracer() const;

protected:
    explicit CanvasBase(IntSize);

    virtual ScriptExecutionContext* canvasBaseScriptExecutionContext() const = 0;

    virtual void setSize(const IntSize& size) { m_size = size; }

    RefPtr<ImageBuffer> setImageBuffer(RefPtr<ImageBuffer>&&) const;
    virtual bool hasCreatedImageBuffer() const { return false; }
    static size_t activePixelMemory();

    void resetGraphicsContextState() const;

private:
    virtual void createImageBuffer() const { }

    mutable IntSize m_size;
    mutable Lock m_imageBufferAssignmentLock;
    mutable RefPtr<ImageBuffer> m_imageBuffer;
    mutable size_t m_imageBufferCost { 0 };
    mutable std::unique_ptr<GraphicsContextStateSaver> m_contextStateSaver;

    bool m_originClean { true };
#if ASSERT_ENABLED
    bool m_didNotifyObserversCanvasDestroyed { false };
#endif
    WeakHashSet<CanvasObserver> m_observers;
    WeakHashSet<CanvasDisplayBufferObserver> m_displayBufferObservers;
};

WebCoreOpaqueRoot root(CanvasBase*);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CANVAS(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
static bool isType(const WebCore::CanvasBase& canvas) { return canvas.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

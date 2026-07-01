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

#include <WebCore/CSSParserContext.h>
#include <WebCore/CanvasNoiseInjection.h>
#include <WebCore/FloatRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/PixelBuffer.h>
#include <WebCore/TaskSource.h>
#include <atomic>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/HashSet.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class AffineTransform;
class CanvasBase;
class CanvasObserver;
class CanvasRenderingContext;
class Element;
class Event;
class GraphicsContext;
class Image;
class ImageBuffer;
class IntRect;
class ScriptExecutionContext;
class SecurityOrigin;
class WebCoreOpaqueRoot;

enum class ShouldApplyPostProcessingToDirtyRect : bool { No, Yes };

class CanvasDisplayBufferObserver : public AbstractRefCountedAndCanMakeWeakPtr<CanvasDisplayBufferObserver> {
public:
    virtual ~CanvasDisplayBufferObserver();

    virtual void canvasDisplayBufferPrepared(CanvasBase&) = 0;
};

class CanvasBase : public AbstractRefCountedAndCanMakeWeakPtr<CanvasBase> {
public:
    virtual ~CanvasBase();

    virtual bool isHTMLCanvasElement() const { return false; }
    virtual bool isOffscreenCanvas() const { return false; }
    virtual bool isCustomPaintCanvas() const { return false; }

    virtual unsigned width() const { return m_size.width(); }
    virtual unsigned height() const { return m_size.height(); }
    const IntSize& size() const { return m_size; }

    ImageBuffer* buffer() const;

    virtual void setImageBufferAndMarkDirty(RefPtr<ImageBuffer>&&) { }

    RefPtr<ImageBuffer> makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect = ShouldApplyPostProcessingToDirtyRect::Yes);

    void setOriginClean() { m_originClean = true; }
    void setOriginTainted() { m_originClean = false; }
    bool originClean() const { return m_originClean; }

    virtual SecurityOrigin* securityOrigin() const { return nullptr; }
    ScriptExecutionContext* scriptExecutionContext() const { return canvasBaseScriptExecutionContext();  }
    RefPtr<ScriptExecutionContext> protectedScriptExecutionContext() const;

    virtual CanvasRenderingContext* renderingContext() const = 0;

    const CSSParserContext& cssParserContext() const;

    void addObserver(CanvasObserver&);
    void removeObserver(CanvasObserver&);
    bool hasObserver(CanvasObserver&) const;
    void notifyObserversCanvasChanged(const FloatRect&);
    void notifyObserversCanvasResized();
    void notifyObserversCanvasDestroyed(); // Must be called in destruction before clearing m_context.
    void addDisplayBufferObserver(CanvasDisplayBufferObserver&);
    void removeDisplayBufferObserver(CanvasDisplayBufferObserver&);
    void notifyObserversCanvasDisplayBufferPrepared();
    bool hasDisplayBufferObservers() const { return !m_displayBufferObservers.isEmptyIgnoringNullReferences(); }

    HashSet<Element*> cssCanvasClients() const;

    // !rect means caller knows the full canvas is invalidated previously.
    void didDraw(const std::optional<FloatRect>& rect) { return didDraw(rect, ShouldApplyPostProcessingToDirtyRect::Yes); }
    virtual void didDraw(const std::optional<FloatRect>&, ShouldApplyPostProcessingToDirtyRect);

    virtual Image* copiedImage() const = 0;
    virtual void clearCopiedImage() const = 0;

    bool hasActiveInspectorCanvasCallTracer() const;

    bool shouldAccelerate(const IntSize&) const;

    WEBCORE_EXPORT static void setMaxCanvasAreaForTesting(std::optional<size_t>);

    virtual void queueTaskKeepingObjectAlive(TaskSource, Function<void(CanvasBase&)>&&) = 0;
    virtual void dispatchEvent(Event&) = 0;

    bool postProcessPixelBufferResults(Ref<PixelBuffer>&&) const;
    void recordLastFillText(const String&);

    void setNoiseInjectionSalt(NoiseInjectionHashSalt salt) { m_canvasNoiseHashSalt = salt; }
    bool havePendingCanvasNoiseInjection() const { return m_canvasNoiseInjection.haveDirtyRects(); }

    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275100): The image buffer from CanvasBase should be moved to CanvasRenderingContext2DBase.
    RefPtr<ImageBuffer> allocateImageBuffer() const;

    void setHasCreatedImageBuffer(bool hasCreatedImageBuffer) { m_hasCreatedImageBuffer = hasCreatedImageBuffer; }
    bool hasCreatedImageBuffer() const { return m_hasCreatedImageBuffer; }

    RefPtr<ImageBuffer> createImageForNoiseInjection() const;

protected:
    explicit CanvasBase(IntSize, ScriptExecutionContext&);

    virtual ScriptExecutionContext* canvasBaseScriptExecutionContext() const = 0;
    RefPtr<ScriptExecutionContext> protectedCanvasBaseScriptExecutionContext() const;
    virtual std::unique_ptr<CSSParserContext> createCSSParserContext() const = 0;

    void setSize(const IntSize&);

    RefPtr<ImageBuffer> setImageBuffer(RefPtr<ImageBuffer>&&) const;
    String lastFillText() const { return m_lastFillText; }
    void addCanvasNeedingPreparationForDisplayOrFlush();
    void removeCanvasNeedingPreparationForDisplayOrFlush();

private:
    bool shouldInjectNoiseBeforeReadback() const;
    virtual void createImageBuffer() const { }
    bool shouldAccelerate(uint64_t area) const;

    mutable IntSize m_size;
    mutable RefPtr<ImageBuffer> m_imageBuffer;
    mutable std::unique_ptr<CSSParserContext> m_cssParserContext;

    String m_lastFillText;

    WeakHashSet<CanvasObserver> m_observers;
    WeakHashSet<CanvasDisplayBufferObserver> m_displayBufferObservers;

    CanvasNoiseInjection m_canvasNoiseInjection;
    Markable<NoiseInjectionHashSalt, IntegralMarkableTraits<NoiseInjectionHashSalt, std::numeric_limits<int64_t>::max()>> m_canvasNoiseHashSalt;

    bool m_originClean { true };
    // m_hasCreatedImageBuffer means we tried to malloc the buffer. We didn't necessarily get it.
    bool m_hasCreatedImageBuffer { false };
#if ASSERT_ENABLED
    bool m_didNotifyObserversCanvasDestroyed { false };
#endif
};

WebCoreOpaqueRoot root(CanvasBase*);


inline const CSSParserContext& CanvasBase::cssParserContext() const
{
    if (!m_cssParserContext) [[unlikely]]
        m_cssParserContext = createCSSParserContext();
    return *m_cssParserContext;
}

} // namespace WebCore

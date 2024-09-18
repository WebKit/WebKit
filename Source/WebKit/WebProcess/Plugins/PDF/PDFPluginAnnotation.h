/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(PDF_PLUGIN)

#include <WebCore/EventListener.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class Document;
class Element;
class WeakPtrImplWithEventTargetData;
}

OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFLayerController;

namespace WebKit {

class PDFPluginBase;

class PDFPluginAnnotation : public RefCounted<PDFPluginAnnotation>, public CanMakeCheckedPtr<PDFPluginAnnotation> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PDFPluginAnnotation);
public:
    static RefPtr<PDFPluginAnnotation> create(PDFAnnotation *, PDFPluginBase*);
    virtual ~PDFPluginAnnotation();

    WebCore::Element* element() const { return m_element.get(); }
    PDFAnnotation *annotation() const { return m_annotation.get(); }
    PDFPluginBase* plugin() const { return m_plugin.get(); }

    virtual void updateGeometry();
    virtual void commit();

    void attach(WebCore::Element*);

protected:
    PDFPluginAnnotation(PDFAnnotation *annotation, PDFPluginBase* plugin)
        : m_annotation(annotation)
        , m_eventListener(PDFPluginAnnotationEventListener::create(this))
        , m_plugin(plugin)
    {
    }

    WebCore::Element* parent() const { return m_parent.get(); }
    WebCore::EventListener* eventListener() const { return m_eventListener.get(); }

    virtual bool handleEvent(WebCore::Event&);

private:
    virtual Ref<WebCore::Element> createAnnotationElement() = 0;

    class PDFPluginAnnotationEventListener : public WebCore::EventListener {
    public:
        static Ref<PDFPluginAnnotationEventListener> create(PDFPluginAnnotation* annotation)
        {
            return adoptRef(*new PDFPluginAnnotationEventListener(annotation));
        }

        void setAnnotation(PDFPluginAnnotation* annotation) { m_annotation = annotation; }

    private:

        PDFPluginAnnotationEventListener(PDFPluginAnnotation* annotation)
            : WebCore::EventListener(WebCore::EventListener::CPPEventListenerType)
            , m_annotation(annotation)
        {
        }

        void handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event&) override;

        CheckedPtr<PDFPluginAnnotation> m_annotation;
    };

    WeakPtr<WebCore::Element, WebCore::WeakPtrImplWithEventTargetData> m_parent;

    RefPtr<WebCore::Element> m_element;
    RetainPtr<PDFAnnotation> m_annotation;

    RefPtr<PDFPluginAnnotationEventListener> m_eventListener;

    RefPtr<PDFPluginBase> m_plugin;
};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)

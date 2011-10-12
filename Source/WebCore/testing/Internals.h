/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Internals_h
#define Internals_h

#include "ExceptionCode.h"
#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ClientRect;
class Document;
class Element;
class Node;
class PopupMenuClient;
class Range;
class RenderObject;

class Internals : public RefCounted<Internals> {
public:
    static PassRefPtr<Internals> create();
    virtual ~Internals();

    void reset(Document*);

    String elementRenderTreeAsText(Element*, ExceptionCode&);

    bool isPreloaded(Document*, const String& url);

    Node* ensureShadowRoot(Element* host, ExceptionCode&);
    Node* shadowRoot(Element* host, ExceptionCode&);
    void removeShadowRoot(Element* host, ExceptionCode&);
    Element* includerFor(Node*, ExceptionCode&);
    String shadowPseudoId(Element*, ExceptionCode&);
    PassRefPtr<Element> createShadowContentElement(Document*, ExceptionCode&);
    Element* getElementByIdInShadowRoot(Node* shadowRoot, const String& id, ExceptionCode&);

#if ENABLE(INPUT_COLOR)
    bool connectColorChooserClient(Element*);
    void selectColorInColorChooser(const String& colorValue);
#endif

#if ENABLE(INSPECTOR)
    void setInspectorResourcesDataSizeLimits(Document*, int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode&);
#else
    void setInspectorResourcesDataSizeLimits(Document*, int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode&) { }
#endif

    PassRefPtr<ClientRect> boundingBox(Element*, ExceptionCode&);

    unsigned markerCountForNode(Node*, ExceptionCode&);
    PassRefPtr<Range> markerRangeForNode(Node*, unsigned, ExceptionCode&);

    void setForceCompositingMode(Document*, bool enabled, ExceptionCode&);

    void setEnableScrollAnimator(Document*, bool enabled, ExceptionCode&);
    void setZoomAnimatorTransform(Document*, float scale, float tx, float ty, ExceptionCode&);
    float getPageScaleFactor(Document*,  ExceptionCode&);
    void setZoomParameters(Document*, float scale, float x, float y, ExceptionCode&);

    void setPasswordEchoEnabled(Document*, bool enabled, ExceptionCode&);
    void setPasswordEchoDurationInSeconds(Document*, double durationInSeconds, ExceptionCode&);

    void setScrollViewPosition(Document*, long x, long y, ExceptionCode&);

    bool wasLastChangeUserEdit(Element* textField, ExceptionCode&);
    String suggestedValue(Element* inputElement, ExceptionCode&);
    void setSuggestedValue(Element* inputElement, const String&, ExceptionCode&);
    void scrollElementToRect(Element*, long x, long y, long w, long h, ExceptionCode&);

    int popupClientPaddingLeft(Element*, ExceptionCode&);
    int popupClientPaddingRight(Element*, ExceptionCode&);
    PassRefPtr<ClientRect> popupClientBoundingBoxRect(Element*, ExceptionCode&);

    static const char* internalsId;

    void paintControlTints(Document*, ExceptionCode&);

private:
    Internals();

    double passwordEchoDurationInSecondsBackup;
    bool passwordEchoEnabledBackup : 1;
    bool passwordEchoDurationInSecondsBackedUp : 1;
    bool passwordEchoEnabledBackedUp : 1;
};

} // namespace WebCore

#endif

/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLMarqueeElement_h
#define HTMLMarqueeElement_h

#include "ActiveDOMObject.h"
#include "HTMLElement.h"

namespace WebCore {

class RenderMarquee;

class HTMLMarqueeElement final : public HTMLElement, private ActiveDOMObject {
public:
    static Ref<HTMLMarqueeElement> create(const QualifiedName&, Document&);

    int minimumDelay() const;

    // DOM Functions

    void start();
    virtual void stop() override;
    
    unsigned scrollAmount() const;
    void setScrollAmount(unsigned);
    
    unsigned scrollDelay() const;
    void setScrollDelay(unsigned);
    
    int loop() const;
    void setLoop(int, ExceptionCode&);
    
private:
    HTMLMarqueeElement(const QualifiedName&, Document&);

    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    // ActiveDOMObject
    virtual bool canSuspendForDocumentSuspension() const override;
    virtual void suspend(ReasonForSuspension) override;
    virtual void resume() override;
    virtual const char* activeDOMObjectName() const override { return "HTMLMarqueeElement"; }

    virtual bool canHaveUserAgentShadowRoot() const override final { return true; }

    RenderMarquee* renderMarquee() const;
};

} // namespace WebCore

#endif // HTMLMarqueeElement_h

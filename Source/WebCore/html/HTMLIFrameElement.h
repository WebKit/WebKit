/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "FeaturePolicy.h"
#include "HTMLFrameElementBase.h"

namespace WebCore {

class DOMTokenList;
class LazyLoadFrameObserver;
class RenderIFrame;

class HTMLIFrameElement final : public HTMLFrameElementBase {
    WTF_MAKE_ISO_ALLOCATED(HTMLIFrameElement);
public:
    static Ref<HTMLIFrameElement> create(const QualifiedName&, Document&);

    DOMTokenList& sandbox();

    RenderIFrame* renderer() const;

    void setReferrerPolicyForBindings(const AtomString&);
    String referrerPolicyForBindings() const;
    ReferrerPolicy referrerPolicy() const final;

    const FeaturePolicy& featurePolicy() const;

    const AtomString& loadingForBindings() const;
    void setLoadingForBindings(const AtomString&);

    LazyLoadFrameObserver& lazyLoadFrameObserver();

    void loadDeferredFrame();

private:
    HTMLIFrameElement(const QualifiedName&, Document&);

    int defaultTabIndex() const final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;

    bool isInteractiveContent() const final { return true; }

    bool rendererIsNeeded(const RenderStyle&) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    bool shouldLoadFrameLazily() final;
    bool isLazyLoadObserverActive() const final;

    std::unique_ptr<DOMTokenList> m_sandbox;
    mutable Optional<FeaturePolicy> m_featurePolicy;
    std::unique_ptr<LazyLoadFrameObserver> m_lazyLoadFrameObserver;
};

} // namespace WebCore

/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "SVGNames.h"
#include <wtf/FastMalloc.h>
#include <wtf/IsoMalloc.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSSVGResourceElementClient;
class Document;
class LegacyRenderSVGResourceClipper;
class LegacyRenderSVGResourceContainer;
class QualifiedName;
class ReferenceFilterOperation;
class ReferencePathOperation;
class RenderElement;
class RenderSVGResourceFilter;
class RenderStyle;
class SVGClipPathElement;
class SVGElement;
class SVGFilterElement;
class SVGMarkerElement;
class SVGMaskElement;
class StyleImage;
class TreeScope;

class ReferencedSVGResources {
    WTF_MAKE_ISO_ALLOCATED(ReferencedSVGResources);
public:
    ReferencedSVGResources(RenderElement&);
    ~ReferencedSVGResources();

    using SVGQualifiedNames = Vector<SVGQualifiedName>;
    using SVGElementIdentifierAndTagPairs = Vector<std::pair<AtomString, SVGQualifiedNames>>;

    static SVGElementIdentifierAndTagPairs referencedSVGResourceIDs(const RenderStyle&, const Document&);
    void updateReferencedResources(TreeScope&, const SVGElementIdentifierAndTagPairs&);

    // Legacy: Clipping needs a renderer, filters use an element.
    static LegacyRenderSVGResourceClipper* referencedClipperRenderer(TreeScope&, const ReferencePathOperation&);
    static RefPtr<SVGFilterElement> referencedFilterElement(TreeScope&, const ReferenceFilterOperation&);

    static LegacyRenderSVGResourceContainer* referencedRenderResource(TreeScope&, const AtomString& fragment);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // LBSE: All element based.
    static RefPtr<SVGClipPathElement> referencedClipPathElement(TreeScope&, const ReferencePathOperation&);
    static RefPtr<SVGMarkerElement> referencedMarkerElement(TreeScope&, const String&);
    static RefPtr<SVGMaskElement> referencedMaskElement(TreeScope&, const StyleImage&);
    static RefPtr<SVGElement> referencedPaintServerElement(TreeScope&, const String&);
#endif

private:
    static RefPtr<SVGElement> elementForResourceID(TreeScope&, const AtomString& resourceID, const SVGQualifiedName& tagName);
    static RefPtr<SVGElement> elementForResourceIDs(TreeScope&, const AtomString& resourceID, const SVGQualifiedNames& tagNames);

    void addClientForTarget(SVGElement& targetElement, const AtomString&);
    void removeClientForTarget(TreeScope&, const AtomString&);

    RenderElement& m_renderer;
    MemoryCompactRobinHoodHashMap<AtomString, std::unique_ptr<CSSSVGResourceElementClient>> m_elementClients;
};

} // namespace WebCore

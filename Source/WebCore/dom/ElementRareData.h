/*
 * Copyright (C) 2008, 2009, 2010, 2014, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#include "Element.h"
#include "NodeRareData.h"
#include "SpaceSplitString.h"
#include <wtf/Markable.h>

namespace WebCore {

class CustomElementDefaultARIA;
class CustomElementReactionQueue;
class DOMTokenList;
class DatasetDOMStringMap;
class ElementAnimationRareData;
class FormAssociatedCustomElement;
class NamedNodeMap;
class PopoverData;
class PseudoElement;
class RenderStyle;
class ShadowRoot;
class StylePropertyMap;
class StylePropertyMapReadOnly;

struct IntersectionObserverData;
struct ResizeObserverData;

struct LayoutUnitMarkableTraits {
    static bool isEmptyValue(LayoutUnit value)
    {
        return value == LayoutUnit(-1);
    }

    static LayoutUnit emptyValue()
    {
        return LayoutUnit(-1);
    }
};

class ElementRareData : public NodeRareData {
public:
    explicit ElementRareData();
    ~ElementRareData();

    void setBeforePseudoElement(RefPtr<PseudoElement>&&);
    void setAfterPseudoElement(RefPtr<PseudoElement>&&);

    PseudoElement* beforePseudoElement() const { return m_beforePseudoElement.get(); }
    PseudoElement* afterPseudoElement() const { return m_afterPseudoElement.get(); }

    void resetComputedStyle();

    int unusualTabIndex() const;
    void setUnusualTabIndex(int);

    unsigned childIndex() const { return m_childIndex; }
    inline void setChildIndex(unsigned);
    static ptrdiff_t childIndexMemoryOffset() { return OBJECT_OFFSETOF(ElementRareData, m_childIndex); }

    inline void clearShadowRoot();
    ShadowRoot* shadowRoot() const { return m_shadowRoot.get(); }
    inline void setShadowRoot(RefPtr<ShadowRoot>&&);

    CustomElementReactionQueue* customElementReactionQueue() { return m_customElementReactionQueue.get(); }
    inline void setCustomElementReactionQueue(std::unique_ptr<CustomElementReactionQueue>&&);

    CustomElementDefaultARIA* customElementDefaultARIA() { return m_customElementDefaultARIA.get(); }
    inline void setCustomElementDefaultARIA(std::unique_ptr<CustomElementDefaultARIA>&&);

    FormAssociatedCustomElement* formAssociatedCustomElement() { return m_formAssociatedCustomElement.get(); }
    inline void setFormAssociatedCustomElement(std::unique_ptr<FormAssociatedCustomElement>&&);

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    inline void setAttributeMap(std::unique_ptr<NamedNodeMap>&&);

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    inline void setComputedStyle(std::unique_ptr<RenderStyle>&&);

    RenderStyle* displayContentsStyle() const { return m_displayContentsStyle.get(); }
    inline void setDisplayContentsStyle(std::unique_ptr<RenderStyle>);

    const AtomString& effectiveLang() const { return m_effectiveLang; }
    inline void setEffectiveLang(const AtomString&);

    DOMTokenList* classList() const { return m_classList.get(); }
    inline void setClassList(std::unique_ptr<DOMTokenList>&&);

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    inline void setDataset(std::unique_ptr<DatasetDOMStringMap>&&);

    IntPoint savedLayerScrollPosition() const { return m_savedLayerScrollPosition; }
    inline void setSavedLayerScrollPosition(IntPoint);

    ElementAnimationRareData* animationRareData(PseudoId) const;
    ElementAnimationRareData& ensureAnimationRareData(PseudoId);

    DOMTokenList* partList() const { return m_partList.get(); }
    inline void setPartList(std::unique_ptr<DOMTokenList>&&);

    const SpaceSplitString& partNames() const { return m_partNames; }
    inline void setPartNames(SpaceSplitString&&);

    IntersectionObserverData* intersectionObserverData() { return m_intersectionObserverData.get(); }
    inline void setIntersectionObserverData(std::unique_ptr<IntersectionObserverData>&&);

    ResizeObserverData* resizeObserverData() { return m_resizeObserverData.get(); }
    inline void setResizeObserverData(std::unique_ptr<ResizeObserverData>&&);

    std::optional<LayoutUnit> lastRememberedLogicalWidth() const { return m_lastRememberedLogicalWidth; }
    std::optional<LayoutUnit> lastRememberedLogicalHeight() const { return m_lastRememberedLogicalHeight; }
    inline void setLastRememberedLogicalWidth(LayoutUnit);
    inline void setLastRememberedLogicalHeight(LayoutUnit);
    void clearLastRememberedLogicalWidth() { m_lastRememberedLogicalWidth.reset(); }
    void clearLastRememberedLogicalHeight() { m_lastRememberedLogicalHeight.reset(); }

    const AtomString& nonce() const { return m_nonce; }
    inline void setNonce(const AtomString&);

    StylePropertyMap* attributeStyleMap() { return m_attributeStyleMap.get(); }
    inline void setAttributeStyleMap(Ref<StylePropertyMap>&&);

    StylePropertyMapReadOnly* computedStyleMap() { return m_computedStyleMap.get(); }
    inline void setComputedStyleMap(Ref<StylePropertyMapReadOnly>&&);

    ExplicitlySetAttrElementsMap& explicitlySetAttrElementsMap() { return m_explicitlySetAttrElementsMap; }

    PopoverData* popoverData() { return m_popoverData.get(); }
    inline void setPopoverData(std::unique_ptr<PopoverData>&&);

#if DUMP_NODE_STATISTICS
    inline OptionSet<UseType> useTypes() const;
#endif

private:
    IntPoint m_savedLayerScrollPosition;
    std::unique_ptr<RenderStyle> m_computedStyle;
    std::unique_ptr<RenderStyle> m_displayContentsStyle;

    AtomString m_effectiveLang;
    std::unique_ptr<DatasetDOMStringMap> m_dataset;
    std::unique_ptr<DOMTokenList> m_classList;
    RefPtr<ShadowRoot> m_shadowRoot;
    std::unique_ptr<CustomElementReactionQueue> m_customElementReactionQueue;
    std::unique_ptr<CustomElementDefaultARIA> m_customElementDefaultARIA;
    std::unique_ptr<FormAssociatedCustomElement> m_formAssociatedCustomElement;
    std::unique_ptr<NamedNodeMap> m_attributeMap;

    std::unique_ptr<IntersectionObserverData> m_intersectionObserverData;

    std::unique_ptr<ResizeObserverData> m_resizeObserverData;

    Markable<LayoutUnit, LayoutUnitMarkableTraits> m_lastRememberedLogicalWidth;
    Markable<LayoutUnit, LayoutUnitMarkableTraits> m_lastRememberedLogicalHeight;

    Vector<std::unique_ptr<ElementAnimationRareData>> m_animationRareData;

    RefPtr<PseudoElement> m_beforePseudoElement;
    RefPtr<PseudoElement> m_afterPseudoElement;

    RefPtr<StylePropertyMap> m_attributeStyleMap;
    RefPtr<StylePropertyMapReadOnly> m_computedStyleMap;

    std::unique_ptr<DOMTokenList> m_partList;
    SpaceSplitString m_partNames;

    AtomString m_nonce;

    ExplicitlySetAttrElementsMap m_explicitlySetAttrElementsMap;

    std::unique_ptr<PopoverData> m_popoverData;

    void releasePseudoElement(PseudoElement*);
};

inline int ElementRareData::unusualTabIndex() const
{
    ASSERT(m_unusualTabIndex); // setUnusualTabIndex must have been called before this.
    return m_unusualTabIndex;
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<ElementRareData*>(rareData());
}

inline ShadowRoot* Node::shadowRoot() const
{
    if (!is<Element>(*this))
        return nullptr;
    return downcast<Element>(*this).shadowRoot();
}

inline ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : nullptr;
}

} // namespace WebCore

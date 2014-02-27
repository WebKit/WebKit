/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "StyleResolver.h"

#include "Attribute.h"
#include "CSSBorderImage.h"
#include "CSSCalculationValue.h"
#include "CSSCursorImageValue.h"
#include "CSSDefaultStyleSheets.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSPageRule.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyNames.h"
#include "CSSReflectValue.h"
#include "CSSSelector.h"
#include "CSSSelectorList.h"
#include "CSSShadowValue.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CSSTimingFunctionValue.h"
#include "CSSValueList.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "CalculationValue.h"
#include "ContentData.h"
#include "Counter.h"
#include "CounterContent.h"
#include "CursorList.h"
#include "DeprecatedStyleBuilder.h"
#include "DocumentStyleSheetCollection.h"
#include "ElementRuleCollector.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLIFrameElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLProgressElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "KeyframeList.h"
#include "LinkHash.h"
#include "LocaleToScriptMapping.h"
#include "MathMLNames.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingTraversal.h"
#include "Page.h"
#include "PageRuleCollector.h"
#include "Pair.h"
#include "QuotesData.h"
#include "Rect.h"
#include "RenderRegion.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderStyleConstants.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "SecurityOrigin.h"
#include "SelectorCheckerFastPath.h"
#include "Settings.h"
#include "ShadowData.h"
#include "ShadowRoot.h"
#include "StyleCachedImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StylePendingImage.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "Text.h"
#include "TransformFunctions.h"
#include "TransformOperations.h"
#include "UserAgentStyleSheets.h"
#include "ViewportStyleResolver.h"
#include "VisitedLinkState.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSTransformValue.h"
#include "WebKitFontFamilyNames.h"
#include "XMLNames.h"
#include <bitset>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if ENABLE(CSS_FILTERS)
#include "FilterOperation.h"
#include "WebKitCSSFilterValue.h"
#endif

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridTemplateAreasValue.h"
#endif

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#include "StyleCachedImageSet.h"
#endif

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLAudioElement.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "WebVTTElement.h"
#endif

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLMediaElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

class StyleResolver::CascadedProperties {
public:
    CascadedProperties(TextDirection, WritingMode);

    struct Property {
        void apply(StyleResolver&);

        CSSPropertyID id;
        CSSValue* cssValue[3];
    };

    bool hasProperty(CSSPropertyID id) const { return m_propertyIsPresent.test(id); }
    Property& property(CSSPropertyID);
    bool addMatches(const MatchResult&, bool important, int startIndex, int endIndex, bool inheritedOnly = false);

    void set(CSSPropertyID, CSSValue&, unsigned linkMatchType);
    void setDeferred(CSSPropertyID, CSSValue&, unsigned linkMatchType);

    void applyDeferredProperties(StyleResolver&);

private:
    bool addStyleProperties(const StyleProperties&, StyleRule&, bool isImportant, bool inheritedOnly, PropertyWhitelistType, unsigned linkMatchType);
    static void setPropertyInternal(Property&, CSSPropertyID, CSSValue&, unsigned linkMatchType);

    Property m_properties[numCSSProperties + 1];
    std::bitset<numCSSProperties + 1> m_propertyIsPresent;

    Vector<Property, 8> m_deferredProperties;

    TextDirection m_direction;
    WritingMode m_writingMode;
};

static void extractDirectionAndWritingMode(const RenderStyle&, const StyleResolver::MatchResult&, TextDirection&, WritingMode&);

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    m_state.style()->set##Prop(m_state.parentStyle()->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_state.style()->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

RenderStyle* StyleResolver::s_styleNotYetAvailable;

inline void StyleResolver::State::cacheBorderAndBackground()
{
    m_hasUAAppearance = m_style->hasAppearance();
    if (m_hasUAAppearance) {
        m_borderData = m_style->border();
        m_backgroundData = *m_style->backgroundLayers();
        m_backgroundColor = m_style->backgroundColor();
    }
}

inline void StyleResolver::State::clear()
{
    m_element = nullptr;
    m_styledElement = nullptr;
    m_parentStyle = nullptr;
    m_parentNode = nullptr;
    m_regionForStyling = nullptr;
    m_pendingImageProperties.clear();
#if ENABLE(CSS_FILTERS)
    m_filtersWithPendingSVGDocuments.clear();
#endif
}

void StyleResolver::MatchResult::addMatchedProperties(const StyleProperties& properties, StyleRule* rule, unsigned linkMatchType, PropertyWhitelistType propertyWhitelistType)
{
    matchedProperties.grow(matchedProperties.size() + 1);
    StyleResolver::MatchedProperties& newProperties = matchedProperties.last();
    newProperties.properties = const_cast<StyleProperties*>(&properties);
    newProperties.linkMatchType = linkMatchType;
    newProperties.whitelistType = propertyWhitelistType;
    matchedRules.append(rule);
}

StyleResolver::StyleResolver(Document& document, bool matchAuthorAndUserStyles)
    : m_matchedPropertiesCacheAdditionsSinceLastSweep(0)
    , m_matchedPropertiesCacheSweepTimer(this, &StyleResolver::sweepMatchedPropertiesCache)
    , m_document(document)
    , m_matchAuthorAndUserStyles(matchAuthorAndUserStyles)
    , m_fontSelector(CSSFontSelector::create(&m_document))
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
#endif
    , m_deprecatedStyleBuilder(DeprecatedStyleBuilder::sharedStyleBuilder())
    , m_styleMap(this)
{
    Element* root = m_document.documentElement();

    CSSDefaultStyleSheets::initDefaultStyle(root);

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = m_document.view();
    if (view)
        m_medium = std::make_unique<MediaQueryEvaluator>(view->mediaType());
    else
        m_medium = std::make_unique<MediaQueryEvaluator>("all");

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, DisallowStyleSharing, MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = std::make_unique<MediaQueryEvaluator>(view->mediaType(), &view->frame(), m_rootDefaultStyle.get());

    m_ruleSets.resetAuthorStyle();

    DocumentStyleSheetCollection& styleSheetCollection = m_document.styleSheetCollection();
    m_ruleSets.initUserStyle(styleSheetCollection, *m_medium, *this);

#if ENABLE(SVG_FONTS)
    if (m_document.svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = m_document.svgExtensions()->svgFontFaceElements();
        HashSet<SVGFontFaceElement*>::const_iterator end = svgFontFaceElements.end();
        for (HashSet<SVGFontFaceElement*>::const_iterator it = svgFontFaceElements.begin(); it != end; ++it)
            fontSelector()->addFontFaceRule((*it)->fontFaceRule());
    }
#endif

    appendAuthorStyleSheets(0, styleSheetCollection.activeAuthorStyleSheets());
}

void StyleResolver::appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet>>& styleSheets)
{
    m_ruleSets.appendAuthorStyleSheets(firstNew, styleSheets, m_medium.get(), m_inspectorCSSOMWrappers, this);
    if (auto renderView = document().renderView())
        renderView->style().font().update(fontSelector());

#if ENABLE(CSS_DEVICE_ADAPTATION)
    viewportStyleResolver()->resolve();
#endif
}

void StyleResolver::pushParentElement(Element* parent)
{
    const ContainerNode* parentsParent = parent->parentOrShadowHostElement();

    // We are not always invoked consistently. For example, script execution can cause us to enter
    // style recalc in the middle of tree building. We may also be invoked from somewhere within the tree.
    // Reset the stack in this case, or if we see a new root element.
    // Otherwise just push the new parent.
    if (!parentsParent || m_selectorFilter.parentStackIsEmpty())
        m_selectorFilter.setupParentStack(parent);
    else
        m_selectorFilter.pushParent(parent);
}

void StyleResolver::popParentElement(Element* parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(parent))
        m_selectorFilter.popParent();
}

// This is a simplified style setting function for keyframe styles
void StyleResolver::addKeyframeStyle(PassRefPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), rule);
}

StyleResolver::~StyleResolver()
{
    m_fontSelector->clearDocument();

#if ENABLE(CSS_DEVICE_ADAPTATION)
    m_viewportStyleResolver->clearDocument();
#endif
}

void StyleResolver::sweepMatchedPropertiesCache(Timer<StyleResolver>*)
{
    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to generate a new inlineStyle()
    // or presentationAttributeStyle(), potentially leaving this cache with the last ref on the old one.
    Vector<unsigned, 16> toRemove;
    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.begin();
    MatchedPropertiesCache::iterator end = m_matchedPropertiesCache.end();
    for (; it != end; ++it) {
        Vector<MatchedProperties>& matchedProperties = it->value.matchedProperties;
        for (size_t i = 0; i < matchedProperties.size(); ++i) {
            if (matchedProperties[i].properties->hasOneRef()) {
                toRemove.append(it->key);
                break;
            }
        }
    }
    for (size_t i = 0; i < toRemove.size(); ++i)
        m_matchedPropertiesCache.remove(toRemove[i]);

    m_matchedPropertiesCacheAdditionsSinceLastSweep = 0;
}

bool StyleResolver::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_ruleSets.features().classesInRules.contains(classNames[i].impl()))
            return true;
    }
    return false;
}

inline void StyleResolver::State::initElement(Element* e)
{
    m_element = e;
    m_styledElement = e && e->isStyledElement() ? toStyledElement(e) : nullptr;
    m_elementLinkState = e ? e->document().visitedLinkState().determineLinkState(e) : NotInsideLink;
}

inline void StyleResolver::initElement(Element* e)
{
    if (m_state.element() != e) {
        m_state.initElement(e);
        if (e && e == e->document().documentElement()) {
            e->document().setDirectionSetOnDocumentElement(false);
            e->document().setWritingModeSetOnDocumentElement(false);
        }
    }
}

inline void StyleResolver::State::initForStyleResolve(Document& document, Element* e, RenderStyle* parentStyle, RenderRegion* regionForStyling)
{
    m_regionForStyling = regionForStyling;

    if (e) {
        m_parentNode = NodeRenderingTraversal::parent(e);
        bool resetStyleInheritance = hasShadowRootParent(*e) && toShadowRoot(e->parentNode())->resetStyleInheritance();
        m_parentStyle = resetStyleInheritance ? 0 :
            parentStyle ? parentStyle :
            m_parentNode ? m_parentNode->renderStyle() : 0;
    } else {
        m_parentNode = 0;
        m_parentStyle = parentStyle;
    }

    Node* docElement = e ? e->document().documentElement() : 0;
    RenderStyle* docStyle = document.renderStyle();
    m_rootElementStyle = docElement && e != docElement ? docElement->renderStyle() : docStyle;

    m_style = 0;
    m_pendingImageProperties.clear();
    m_fontDirty = false;
}

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

static inline bool parentElementPreventsSharing(const Element* parentElement)
{
    if (!parentElement)
        return false;
    return parentElement->hasFlagsSetDuringStylingOfChildren();
}

Node* StyleResolver::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
{
    if (visitedNodeCount >= cStyleSearchThreshold * cStyleSearchLevelThreshold)
        return 0;
    if (!parent || !parent->isStyledElement())
        return 0;
    StyledElement* p = toStyledElement(parent);
    if (p->inlineStyle())
        return 0;
    if (p->isSVGElement() && toSVGElement(p)->animatedSMILStyleProperties())
        return 0;
    if (p->hasID() && m_ruleSets.features().idsInRules.contains(p->idForStyleResolution().impl()))
        return 0;

    RenderStyle* parentStyle = p->renderStyle();
    unsigned subcount = 0;
    Node* thisCousin = p;
    Node* currentNode = p->previousSibling();

    // Reserve the tries for this level. This effectively makes sure that the algorithm
    // will never go deeper than cStyleSearchLevelThreshold levels into recursion.
    visitedNodeCount += cStyleSearchThreshold;
    while (thisCousin) {
        while (currentNode) {
            ++subcount;
            if (currentNode->renderStyle() == parentStyle && currentNode->lastChild()
                && currentNode->isElementNode() && !parentElementPreventsSharing(toElement(currentNode))
                ) {
                // Adjust for unused reserved tries.
                visitedNodeCount -= cStyleSearchThreshold - subcount;
                return currentNode->lastChild();
            }
            if (subcount >= cStyleSearchThreshold)
                return 0;
            currentNode = currentNode->previousSibling();
        }
        currentNode = locateCousinList(thisCousin->parentElement(), visitedNodeCount);
        thisCousin = currentNode;
    }

    return 0;
}

bool StyleResolver::styleSharingCandidateMatchesRuleSet(RuleSet* ruleSet)
{
    if (!ruleSet)
        return false;

    ElementRuleCollector collector(*m_state.element(), m_state.style(), m_ruleSets, m_selectorFilter);
    return collector.hasAnyMatchingRules(ruleSet);
}

bool StyleResolver::canShareStyleWithControl(StyledElement* element) const
{
    const State& state = m_state;
    HTMLInputElement* thisInputElement = element->toInputElement();
    HTMLInputElement* otherInputElement = state.element()->toInputElement();

    if (!thisInputElement || !otherInputElement)
        return false;

    if (thisInputElement->elementData() != otherInputElement->elementData()) {
        if (thisInputElement->fastGetAttribute(typeAttr) != otherInputElement->fastGetAttribute(typeAttr))
            return false;
        if (thisInputElement->fastGetAttribute(readonlyAttr) != otherInputElement->fastGetAttribute(readonlyAttr))
            return false;
    }

    if (thisInputElement->isAutofilled() != otherInputElement->isAutofilled())
        return false;
    if (thisInputElement->shouldAppearChecked() != otherInputElement->shouldAppearChecked())
        return false;
    if (thisInputElement->shouldAppearIndeterminate() != otherInputElement->shouldAppearIndeterminate())
        return false;
    if (thisInputElement->isRequired() != otherInputElement->isRequired())
        return false;

    if (element->isDisabledFormControl() != state.element()->isDisabledFormControl())
        return false;

    if (element->isDefaultButtonForForm() != state.element()->isDefaultButtonForForm())
        return false;

    if (state.document().containsValidityStyleRules()) {
        bool willValidate = element->willValidate();

        if (willValidate != state.element()->willValidate())
            return false;

        if (willValidate && (element->isValidFormControlElement() != state.element()->isValidFormControlElement()))
            return false;

        if (element->isInRange() != state.element()->isInRange())
            return false;

        if (element->isOutOfRange() != state.element()->isOutOfRange())
            return false;
    }

    return true;
}

static inline bool elementHasDirectionAuto(Element* element)
{
    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    return element->isHTMLElement() && toHTMLElement(element)->hasDirectionAuto();
}

bool StyleResolver::sharingCandidateHasIdenticalStyleAffectingAttributes(StyledElement* sharingCandidate) const
{
    const State& state = m_state;
    if (state.element()->elementData() == sharingCandidate->elementData())
        return true;
    if (state.element()->fastGetAttribute(XMLNames::langAttr) != sharingCandidate->fastGetAttribute(XMLNames::langAttr))
        return false;
    if (state.element()->fastGetAttribute(langAttr) != sharingCandidate->fastGetAttribute(langAttr))
        return false;

    if (!state.elementAffectedByClassRules()) {
        if (sharingCandidate->hasClass() && classNamesAffectedByRules(sharingCandidate->classNames()))
            return false;
    } else if (sharingCandidate->hasClass()) {
        // SVG elements require a (slow!) getAttribute comparision because "class" is an animatable attribute for SVG.
        if (state.element()->isSVGElement()) {
            if (state.element()->getAttribute(classAttr) != sharingCandidate->getAttribute(classAttr))
                return false;
        } else {
            if (state.element()->classNames() != sharingCandidate->classNames())
                return false;
        }
    } else
        return false;

    if (state.styledElement()->presentationAttributeStyle() != sharingCandidate->presentationAttributeStyle())
        return false;

#if ENABLE(PROGRESS_ELEMENT)
    if (state.element()->hasTagName(progressTag)) {
        if (state.element()->shouldAppearIndeterminate() != sharingCandidate->shouldAppearIndeterminate())
            return false;
    }
#endif

    return true;
}

bool StyleResolver::canShareStyleWithElement(StyledElement* element) const
{
    RenderStyle* style = element->renderStyle();
    const State& state = m_state;

    if (!style)
        return false;
    if (style->unique())
        return false;
    if (style->hasUniquePseudoStyle())
        return false;
    if (element->tagQName() != state.element()->tagQName())
        return false;
    if (element->inlineStyle())
        return false;
    if (element->needsStyleRecalc())
        return false;
    if (element->isSVGElement() && toSVGElement(element)->animatedSMILStyleProperties())
        return false;
    if (element->isLink() != state.element()->isLink())
        return false;
    if (element->hovered() != state.element()->hovered())
        return false;
    if (element->active() != state.element()->active())
        return false;
    if (element->focused() != state.element()->focused())
        return false;
    if (element->shadowPseudoId() != state.element()->shadowPseudoId())
        return false;
    if (element == element->document().cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(element))
        return false;
    if (element->additionalPresentationAttributeStyle() != state.styledElement()->additionalPresentationAttributeStyle())
        return false;

    if (element->hasID() && m_ruleSets.features().idsInRules.contains(element->idForStyleResolution().impl()))
        return false;

    // FIXME: We should share style for option and optgroup whenever possible.
    // Before doing so, we need to resolve issues in HTMLSelectElement::recalcListItems
    // and RenderMenuList::setText. See also https://bugs.webkit.org/show_bug.cgi?id=88405
    if (isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
        return false;

    bool isControl = element->isFormControlElement();

    if (isControl != state.element()->isFormControlElement())
        return false;

    if (isControl && !canShareStyleWithControl(element))
        return false;

    if (style->transitions() || style->animations())
        return false;

    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (element->hasTagName(iframeTag) || element->hasTagName(frameTag) || element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag) || element->hasTagName(canvasTag))
        return false;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    // With proxying, the media elements are backed by a RenderEmbeddedObject.
    if ((element->hasTagName(videoTag) || element->hasTagName(audioTag)) && toHTMLMediaElement(element)->shouldUseVideoPluginProxy())
        return false;
#endif

    if (elementHasDirectionAuto(element))
        return false;

    if (element->isLink() && state.elementLinkState() != style->insideLink())
        return false;

#if ENABLE(VIDEO_TRACK)
    // Deny sharing styles between WebVTT and non-WebVTT nodes.
    if (element->isWebVTTElement() != state.element()->isWebVTTElement())
        return false;

    if (element->isWebVTTElement() && state.element()->isWebVTTElement() && toWebVTTElement(element)->isPastNode() != toWebVTTElement(state.element())->isPastNode())
        return false;
#endif

#if ENABLE(FULLSCREEN_API)
    if (element == element->document().webkitCurrentFullScreenElement() || state.element() == state.document().webkitCurrentFullScreenElement())
        return false;
#endif
    return true;
}

inline StyledElement* StyleResolver::findSiblingForStyleSharing(Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!node->isStyledElement())
            continue;
        if (canShareStyleWithElement(toStyledElement(node)))
            break;
        if (count++ == cStyleSearchThreshold)
            return 0;
    }
    return toStyledElement(node);
}

RenderStyle* StyleResolver::locateSharedStyle()
{
    State& state = m_state;
    if (!state.styledElement() || !state.parentStyle())
        return 0;

    // If the element has inline style it is probably unique.
    if (state.styledElement()->inlineStyle())
        return 0;
    if (state.styledElement()->isSVGElement() && toSVGElement(state.styledElement())->animatedSMILStyleProperties())
        return 0;
    // Ids stop style sharing if they show up in the stylesheets.
    if (state.styledElement()->hasID() && m_ruleSets.features().idsInRules.contains(state.styledElement()->idForStyleResolution().impl()))
        return 0;
    if (parentElementPreventsSharing(state.element()->parentElement()))
        return 0;
    if (state.element() == state.document().cssTarget())
        return 0;
    if (elementHasDirectionAuto(state.element()))
        return 0;

    // Cache whether state.element is affected by any known class selectors.
    // FIXME: This shouldn't be a member variable. The style sharing code could be factored out of StyleResolver.
    state.setElementAffectedByClassRules(state.element() && state.element()->hasClass() && classNamesAffectedByRules(state.element()->classNames()));

    // Check previous siblings and their cousins.
    unsigned count = 0;
    unsigned visitedNodeCount = 0;
    StyledElement* shareElement = 0;
    Node* cousinList = state.styledElement()->previousSibling();
    while (cousinList) {
        shareElement = findSiblingForStyleSharing(cousinList, count);
        if (shareElement)
            break;
        cousinList = locateCousinList(cousinList->parentElement(), visitedNodeCount);
    }

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (styleSharingCandidateMatchesRuleSet(m_ruleSets.sibling()))
        return 0;
    // Can't share if attribute rules apply.
    if (styleSharingCandidateMatchesRuleSet(m_ruleSets.uncommonAttribute()))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentElementPreventsSharing(state.element()->parentElement()))
        return 0;
    return shareElement->renderStyle();
}

static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

PassRef<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent,
    StyleSharingBehavior sharingBehavior, RuleMatchingBehavior matchingBehavior, RenderRegion* regionForStyling)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !element->document().haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = &RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        element->document().setHasNodesWithPlaceholderStyle();
        return *s_styleNotYetAvailable;
    }

    State& state = m_state;
    initElement(element);
    state.initForStyleResolve(document(), element, defaultParent, regionForStyling);
    if (sharingBehavior == AllowStyleSharing) {
        if (RenderStyle* sharedStyle = locateSharedStyle()) {
            state.clear();
            return *sharedStyle;
        }
    }

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle(), isAtShadowBoundary(element) ? RenderStyle::AtShadowBoundary : RenderStyle::NotAtShadowBoundary);
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    if (element->isLink()) {
        state.style()->setIsLink(true);
        EInsideLink linkState = state.elementLinkState();
        if (linkState != NotInsideLink) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoVisited);
            if (forceVisited)
                linkState = InsideVisitedLink;
        }
        state.style()->setInsideLink(linkState);
    }

    bool needsCollection = false;
    CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(element, needsCollection);
    if (needsCollection)
        m_ruleSets.collectFeatures();

    ElementRuleCollector collector(*element, state.style(), m_ruleSets, m_selectorFilter);
    collector.setRegionForStyling(regionForStyling);
    collector.setMedium(m_medium.get());

    if (matchingBehavior == MatchOnlyUserAgentRules)
        collector.matchUARules();
    else
        collector.matchAllRules(m_matchAuthorAndUserStyles, matchingBehavior != MatchAllRulesExcludingSMIL);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *state.parentStyle(), element);

    state.clear(); // Clear out for the next resolve.

    document().didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRef<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    MatchResult result;
    result.addMatchedProperties(keyframe->properties());

    ASSERT(!m_state.style());

    State& state = m_state;

    // Create the style
    state.setStyle(RenderStyle::clone(elementStyle));
    state.setLineHeightValue(0);

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state.style(), result, direction, writingMode);

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    CascadedProperties cascade(direction, writingMode);
    cascade.addMatches(result, false, 0, result.matchedProperties.size() - 1);

    applyCascadedProperties(cascade, firstCSSProperty, CSSPropertyLineHeight);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // Now do rest of the properties.
    applyCascadedProperties(cascade, CSSPropertyBackground, lastCSSProperty);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont();

    cascade.applyDeferredProperties(*this);

    // Start loading resources referenced by this style.
    loadPendingResources();
    
    // Add all the animating properties to the keyframe.
    unsigned propertyCount = keyframe->properties().propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSPropertyID property = keyframe->properties().propertyAt(i).id();
        // Timing-function within keyframes is special, because it is not animated; it just
        // describes the timing function between this keyframe and the next.
        if (property != CSSPropertyWebkitAnimationTimingFunction)
            keyframeValue.addProperty(property);
    }

    document().didAccessStyleResolver();

    return state.takeStyle();
}

void StyleResolver::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const StyleRuleKeyframes* keyframesRule = it->value.get();

    // Construct and populate the style for each keyframe
    const Vector<RefPtr<StyleKeyframe>>& keyframes = keyframesRule->keyframes();
    for (unsigned i = 0; i < keyframes.size(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        initElement(e);
        m_state.initForStyleResolve(document(), e);

        const StyleKeyframe* keyframe = keyframes[i].get();

        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, keyframe, keyframeValue));

        // Add this keyframe style to all the indicated key times
        Vector<double> keys;
        keyframe->getKeys(keys);
        for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            keyframeValue.setKey(keys[keyIndex]);
            list.insert(keyframeValue);
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe)
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key()) {
        static StyleKeyframe* zeroPercentKeyframe;
        if (!zeroPercentKeyframe) {
            zeroPercentKeyframe = StyleKeyframe::create(MutableStyleProperties::create()).leakRef();
            zeroPercentKeyframe->setKeyText("0%");
        }
        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, zeroPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe)
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        static StyleKeyframe* hundredPercentKeyframe;
        if (!hundredPercentKeyframe) {
            hundredPercentKeyframe = StyleKeyframe::create(MutableStyleProperties::create()).leakRef();
            hundredPercentKeyframe->setKeyText("100%");
        }
        KeyframeValue keyframeValue(1, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* element, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!element)
        return 0;

    State& state = m_state;

    initElement(element);

    state.initForStyleResolve(document(), element, parentStyle);

    if (m_state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(m_state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.

    // Check UA, user and author rules.
    ElementRuleCollector collector(*element, m_state.style(), m_ruleSets, m_selectorFilter);
    collector.setPseudoStyleRequest(pseudoStyleRequest);
    collector.setMedium(m_medium.get());
    collector.matchUARules();

    if (m_matchAuthorAndUserStyles) {
        collector.matchUserRules(false);
        collector.matchAuthorRules(false);
    }

    if (collector.matchedResult().matchedProperties.isEmpty())
        return 0;

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(*state.style(), *m_state.parentStyle(), 0);

    // Start loading resources referenced by this style.
    loadPendingResources();

    document().didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRef<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    m_state.initForStyleResolve(document(), document().documentElement()); // m_rootElementStyle will be set to the document style.

    m_state.setStyle(RenderStyle::create());
    m_state.style()->inheritFrom(m_state.rootElementStyle());

    PageRuleCollector collector(m_state, m_ruleSets);
    collector.matchAllPageRules(pageIndex);
    m_state.setLineHeightValue(0);

    MatchResult& result = collector.matchedResult();

    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*m_state.style(), result, direction, writingMode);

    CascadedProperties cascade(direction, writingMode);
    cascade.addMatches(result, false, 0, result.matchedProperties.size() - 1);

    applyCascadedProperties(cascade, firstCSSProperty, CSSPropertyLineHeight);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (m_state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, m_state.lineHeightValue());

    applyCascadedProperties(cascade, CSSPropertyBackground, lastCSSProperty);

    cascade.applyDeferredProperties(*this);

    // Start loading resources referenced by this style.
    loadPendingResources();

    document().didAccessStyleResolver();

    // Now return the style.
    return m_state.takeStyle();
}

PassRef<RenderStyle> StyleResolver::defaultStyleForElement()
{
    m_state.setStyle(RenderStyle::create());
    // Make sure our fonts are initialized if we don't inherit them from our parent style.
    if (Settings* settings = documentSettings()) {
        initializeFontStyle(settings);
        m_state.style()->font().update(fontSelector());
    } else
        m_state.style()->font().update(0);

    return m_state.takeStyle();
}

static void addIntrinsicMargins(RenderStyle& style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2 * style.effectiveZoom();

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "quirk" to decide the margin wasn't set is kind of lame.
    if (style.width().isIntrinsicOrAuto()) {
        if (style.marginLeft().quirk())
            style.setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style.marginRight().quirk())
            style.setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style.height().isAuto()) {
        if (style.marginTop().quirk())
            style.setMarginTop(Length(intrinsicMargin, Fixed));
        if (style.marginBottom().quirk())
            style.setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

static EDisplay equivalentBlockDisplay(EDisplay display, bool isFloating, bool strictParsing)
{
    switch (display) {
    case BLOCK:
    case TABLE:
    case BOX:
    case FLEX:
#if ENABLE(CSS_GRID_LAYOUT)
    case GRID:
#endif
        return display;

    case LIST_ITEM:
        // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk, but only in quirks mode.
        if (!strictParsing && isFloating)
            return BLOCK;
        return display;
    case INLINE_TABLE:
        return TABLE;
    case INLINE_BOX:
        return BOX;
    case INLINE_FLEX:
        return FLEX;
#if ENABLE(CSS_GRID_LAYOUT)
    case INLINE_GRID:
        return GRID;
#endif

    case INLINE:
    case COMPACT:
    case INLINE_BLOCK:
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
    case TABLE_ROW:
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
    case TABLE_CELL:
    case TABLE_CAPTION:
        return BLOCK;
    case NONE:
        ASSERT_NOT_REACHED();
        return NONE;
    }
    ASSERT_NOT_REACHED();
    return BLOCK;
}

// CSS requires text-decoration to be reset at each DOM element for tables, 
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements.
static bool doesNotInheritTextDecoration(const RenderStyle& style, Element* e)
{
    return style.display() == TABLE || style.display() == INLINE_TABLE
        || style.display() == INLINE_BLOCK || style.display() == INLINE_BOX || isAtShadowBoundary(e)
        || style.isFloating() || style.hasOutOfFlowPosition();
}

static bool isDisplayFlexibleBox(EDisplay display)
{
    return display == FLEX || display == INLINE_FLEX;
}

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
static bool isScrollableOverflow(EOverflow overflow)
{
    return overflow == OSCROLL || overflow == OAUTO || overflow == OOVERLAY;
}
#endif

void StyleResolver::adjustRenderStyle(RenderStyle& style, const RenderStyle& parentStyle, Element *e)
{
    // Cache our original display.
    style.setOriginalDisplay(style.display());

    if (style.display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
        // these tags to retain their display types.
        if (document().inQuirksMode() && e) {
            if (e->hasTagName(tdTag)) {
                style.setDisplay(TABLE_CELL);
                style.setFloating(NoFloat);
            } else if (isHTMLTableElement(e))
                style.setDisplay(style.isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        if (e && (e->hasTagName(tdTag) || e->hasTagName(thTag))) {
            if (style.whiteSpace() == KHTML_NOWRAP) {
                // Figure out if we are really nowrapping or if we should just
                // use normal instead. If the width of the cell is fixed, then
                // we don't actually use NOWRAP.
                if (style.width().isFixed())
                    style.setWhiteSpace(NORMAL);
                else
                    style.setWhiteSpace(NOWRAP);
            }
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (e && isHTMLTableElement(e) && (style.textAlign() == WEBKIT_LEFT || style.textAlign() == WEBKIT_CENTER || style.textAlign() == WEBKIT_RIGHT))
            style.setTextAlign(TASTART);

        // Frames and framesets never honor position:relative or position:absolute. This is necessary to
        // fix a crash where a site tries to position these objects. They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style.setPosition(StaticPosition);
            style.setDisplay(BLOCK);
        }

        // Ruby text does not support float or position. This might change with evolution of the specification.
        if (e && e->hasTagName(rtTag)) {
            style.setPosition(StaticPosition);
            style.setFloating(NoFloat);
        }

        // FIXME: We shouldn't be overriding start/-webkit-auto like this. Do it in html.css instead.
        // Table headers with a text-align of -webkit-auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style.textAlign() == TASTART)
            style.setTextAlign(CENTER);

        if (e && e->hasTagName(legendTag))
            style.setDisplay(BLOCK);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style.hasOutOfFlowPosition() || style.isFloating() || (e && e->document().documentElement() == e))
            style.setDisplay(equivalentBlockDisplay(style.display(), style.isFloating(), !document().inQuirksMode()));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style.display() == INLINE && style.styleType() == NOPSEUDO && style.writingMode() != parentStyle.writingMode())
            style.setDisplay(INLINE_BLOCK);

        // After performing the display mutation, check table rows. We do not honor position:relative or position:sticky on
        // table rows or cells. This has been established for position:relative in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW_GROUP
            || style.display() == TABLE_FOOTER_GROUP || style.display() == TABLE_ROW)
            && style.position() == RelativePosition)
            style.setPosition(StaticPosition);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style.display() == TABLE_COLUMN || style.display() == TABLE_COLUMN_GROUP || style.display() == TABLE_FOOTER_GROUP
            || style.display() == TABLE_HEADER_GROUP || style.display() == TABLE_ROW || style.display() == TABLE_ROW_GROUP
            || style.display() == TABLE_CELL)
            style.setWritingMode(parentStyle.writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style.writingMode() != TopToBottomWritingMode && (style.display() == BOX || style.display() == INLINE_BOX))
            style.setWritingMode(TopToBottomWritingMode);

        if (isDisplayFlexibleBox(parentStyle.display())) {
            style.setFloating(NoFloat);
            style.setDisplay(equivalentBlockDisplay(style.display(), style.isFloating(), !document().inQuirksMode()));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style.position() == StaticPosition && !isDisplayFlexibleBox(parentStyle.display()))
        style.setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style.hasAutoZIndex() && ((e && e->document().documentElement() == e)
        || style.opacity() < 1.0f
        || style.hasTransformRelatedProperty()
        || style.hasMask()
        || style.clipPath()
        || style.boxReflect()
        || style.hasFilter()
        || style.hasBlendMode()
        || style.hasIsolation()
        || style.position() == StickyPosition
        || (style.position() == FixedPosition && e && e->document().page() && e->document().page()->settings().fixedPositionCreatesStackingContext())
        || style.hasFlowFrom()
        ))
        style.setZIndex(0);

    // Textarea considers overflow visible as auto.
    if (e && isHTMLTextAreaElement(e)) {
        style.setOverflowX(style.overflowX() == OVISIBLE ? OAUTO : style.overflowX());
        style.setOverflowY(style.overflowY() == OVISIBLE ? OAUTO : style.overflowY());
    }

    // Disallow -webkit-user-modify on :pseudo and ::pseudo elements.
    if (e && !e->shadowPseudoId().isNull())
        style.setUserModify(READ_ONLY);

    if (doesNotInheritTextDecoration(style, e))
        style.setTextDecorationsInEffect(style.textDecoration());
    else
        style.addToTextDecorationsInEffect(style.textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style.overflowX() == OMARQUEE && style.overflowY() != OMARQUEE)
        style.setOverflowY(OMARQUEE);
    else if (style.overflowY() == OMARQUEE && style.overflowX() != OMARQUEE)
        style.setOverflowX(OMARQUEE);
    else if (style.overflowX() == OVISIBLE && style.overflowY() != OVISIBLE) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style.setOverflowX(OAUTO);
    } else if (style.overflowY() == OVISIBLE && style.overflowX() != OVISIBLE)
        style.setOverflowY(OAUTO);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // Style::createForDocument().
    if ((style.overflowY() == OPAGEDX || style.overflowY() == OPAGEDY) && !(e && (e->hasTagName(htmlTag) || e->hasTagName(bodyTag))))
        style.setColumnStylesFromPaginationMode(WebCore::paginationModeForRenderStyle(style));

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style.display() == TABLE || style.display() == INLINE_TABLE
        || style.display() == TABLE_ROW_GROUP || style.display() == TABLE_ROW) {
        if (style.overflowX() != OVISIBLE && style.overflowX() != OHIDDEN)
            style.setOverflowX(OVISIBLE);
        if (style.overflowY() != OVISIBLE && style.overflowY() != OHIDDEN)
            style.setOverflowY(OVISIBLE);
    }

    // Menulists should have visible overflow
    if (style.appearance() == MenulistPart) {
        style.setOverflowX(OVISIBLE);
        style.setOverflowY(OVISIBLE);
    }

#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    // Touch overflow scrolling creates a stacking context.
    if (style.hasAutoZIndex() && style.useTouchOverflowScrolling() && (isScrollableOverflow(style.overflowX()) || isScrollableOverflow(style.overflowY())))
        style.setZIndex(0);
#endif

    // Cull out any useless layers and also repeat patterns into additional layers.
    style.adjustBackgroundLayers();
    style.adjustMaskLayers();

    // Do the same for animations and transitions.
    style.adjustAnimations();
    style.adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isFormControlElement() && style.fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!isHTMLInputElement(e) || !toHTMLInputElement(e)->isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style.hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(*this, style, e, m_state.hasUAAppearance(), m_state.borderData(), m_state.backgroundData(), m_state.backgroundColor());

    // If we have first-letter pseudo style, do not share this style.
    if (style.hasPseudoStyle(FIRST_LETTER))
        style.setUnique();

    // FIXME: when dropping the -webkit prefix on transform-style, we should also have opacity < 1 cause flattening.
    if (style.preserves3D() && (style.overflowX() != OVISIBLE
        || style.overflowY() != OVISIBLE
        || style.hasFilter()
        || style.hasBlendMode()))
        style.setTransformStyle3D(TransformStyle3DFlat);

#if ENABLE(CSS_GRID_LAYOUT)
    adjustGridItemPosition(style, parentStyle);
#endif

    if (e && e->isSVGElement()) {
        // Spec: http://www.w3.org/TR/SVG/masking.html#OverflowProperty
        if (style.overflowY() == OSCROLL)
            style.setOverflowY(OHIDDEN);
        else if (style.overflowY() == OAUTO)
            style.setOverflowY(OVISIBLE);

        if (style.overflowX() == OSCROLL)
            style.setOverflowX(OHIDDEN);
        else if (style.overflowX() == OAUTO)
            style.setOverflowX(OVISIBLE);

        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style.setPosition(RenderStyle::initialPosition());

        // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
        // not be scaled again.
        if (e->hasTagName(SVGNames::foreignObjectTag))
            style.setEffectiveZoom(RenderStyle::initialZoom());

        // SVG text layout code expects us to be a block-level style element.
        if ((e->hasTagName(SVGNames::foreignObjectTag) || e->hasTagName(SVGNames::textTag)) && style.isDisplayInlineType())
            style.setDisplay(BLOCK);

        // SVG text layout code expects us to be an inline-level style element.
        if ((e->hasTagName(SVGNames::tspanTag) || e->hasTagName(SVGNames::textPathTag)) && style.display() != NONE && !style.isDisplayInlineType())
            style.setDisplay(INLINE);
    }
}

#if ENABLE(CSS_GRID_LAYOUT)
void StyleResolver::adjustGridItemPosition(RenderStyle& style, const RenderStyle& parentStyle) const
{
    const GridPosition& columnStartPosition = style.gridItemColumnStart();
    const GridPosition& columnEndPosition = style.gridItemColumnEnd();
    const GridPosition& rowStartPosition = style.gridItemRowStart();
    const GridPosition& rowEndPosition = style.gridItemRowEnd();

    // If opposing grid-placement properties both specify a grid span, they both compute to ‘auto’.
    if (columnStartPosition.isSpan() && columnEndPosition.isSpan()) {
        style.setGridItemColumnStart(GridPosition());
        style.setGridItemColumnEnd(GridPosition());
    }

    if (rowStartPosition.isSpan() && rowEndPosition.isSpan()) {
        style.setGridItemRowStart(GridPosition());
        style.setGridItemRowEnd(GridPosition());
    }

    // Unknown named grid area compute to 'auto'.
    const NamedGridAreaMap& map = parentStyle.namedGridArea();

#define CLEAR_UNKNOWN_NAMED_AREA(prop, Prop) \
    if (prop.isNamedGridArea() && !map.contains(prop.namedGridLine())) \
        style.setGridItem##Prop(GridPosition());

    CLEAR_UNKNOWN_NAMED_AREA(columnStartPosition, ColumnStart);
    CLEAR_UNKNOWN_NAMED_AREA(columnEndPosition, ColumnEnd);
    CLEAR_UNKNOWN_NAMED_AREA(rowStartPosition, RowStart);
    CLEAR_UNKNOWN_NAMED_AREA(rowEndPosition, RowEnd);
}
#endif /* ENABLE(CSS_GRID_LAYOUT) */

bool StyleResolver::checkRegionStyle(Element* regionElement)
{
    unsigned rulesSize = m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().size();
    for (unsigned i = 0; i < rulesSize; ++i) {
        ASSERT(m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().at(i).ruleSet.get());
        if (checkRegionSelector(m_ruleSets.authorStyle()->regionSelectorsAndRuleSets().at(i).selector, regionElement))
            return true;
    }

    if (m_ruleSets.userStyle()) {
        rulesSize = m_ruleSets.userStyle()->regionSelectorsAndRuleSets().size();
        for (unsigned i = 0; i < rulesSize; ++i) {
            ASSERT(m_ruleSets.userStyle()->regionSelectorsAndRuleSets().at(i).ruleSet.get());
            if (checkRegionSelector(m_ruleSets.userStyle()->regionSelectorsAndRuleSets().at(i).selector, regionElement))
                return true;
        }
    }

    return false;
}

static void checkForOrientationChange(RenderStyle* style)
{
    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    style->getFontAndGlyphOrientation(fontOrientation, glyphOrientation);

    const FontDescription& fontDescription = style->fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    FontDescription newFontDescription(fontDescription);
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::updateFont()
{
    if (!m_state.fontDirty())
        return;

    RenderStyle* style = m_state.style();
#if ENABLE(IOS_TEXT_AUTOSIZING)
    checkForTextSizeAdjust(style);
#endif
    checkForGenericFamilyChange(style, m_state.parentStyle());
    checkForZoomChange(style, m_state.parentStyle());
    checkForOrientationChange(style);
    style->font().update(m_fontSelector);
    m_state.setFontDirty(false);
}

Vector<RefPtr<StyleRuleBase>> StyleResolver::styleRulesForElement(Element* e, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(e, NOPSEUDO, rulesToInclude);
}

Vector<RefPtr<StyleRuleBase>> StyleResolver::pseudoStyleRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!element || !element->document().haveStylesheetsLoaded())
        return Vector<RefPtr<StyleRuleBase>>();

    initElement(element);
    m_state.initForStyleResolve(document(), element, 0);

    ElementRuleCollector collector(*element, m_state.style(), m_ruleSets, m_selectorFilter);
    collector.setMode(SelectorChecker::CollectingRules);
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));
    collector.setMedium(m_medium.get());

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        collector.matchUARules();
        
        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            collector.matchUserRules(rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules)) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));

        // Check the rules in author sheets.
        collector.matchAuthorRules(rulesToInclude & EmptyCSSRules);
    }

    return collector.matchedRuleList();
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

Length StyleResolver::convertToIntLength(const CSSPrimitiveValue* primitiveValue, const RenderStyle* style, const RenderStyle* rootStyle, double multiplier)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | FractionConversion | ViewportPercentageConversion>(style, rootStyle, multiplier) : Length(Undefined);
}

Length StyleResolver::convertToFloatLength(const CSSPrimitiveValue* primitiveValue, const RenderStyle* style, const RenderStyle* rootStyle, double multiplier)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion | FractionConversion | ViewportPercentageConversion>(style, rootStyle, multiplier) : Length(Undefined);
}

static bool shouldApplyPropertyInParseOrder(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyWebkitTextDecorationLine:
    case CSSPropertyWebkitTextDecorationStyle:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextDecorationSkip:
    case CSSPropertyWebkitTextUnderlinePosition:
    case CSSPropertyTextDecoration:
        return true;
    default:
        return false;
    }
}

static bool elementTypeHasAppearanceFromUAStyle(const Element& element)
{
    // NOTE: This is just a hard-coded list of elements that have some -webkit-appearance value in html.css
    const auto& localName = element.localName();
    return localName == HTMLNames::inputTag
        || localName == HTMLNames::textareaTag
        || localName == HTMLNames::buttonTag
        || localName == HTMLNames::progressTag
        || localName == HTMLNames::selectTag
        || localName == HTMLNames::meterTag
        || localName == HTMLNames::isindexTag;
}

unsigned StyleResolver::computeMatchedPropertiesHash(const MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

bool operator==(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return a.firstUARule == b.firstUARule
        && a.lastUARule == b.lastUARule
        && a.firstAuthorRule == b.firstAuthorRule
        && a.lastAuthorRule == b.lastAuthorRule
        && a.firstUserRule == b.firstUserRule
        && a.lastUserRule == b.lastUserRule;
}

bool operator!=(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return !(a == b);
}

bool operator==(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return a.properties == b.properties && a.linkMatchType == b.linkMatchType;
}

bool operator!=(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return !(a == b);
}

const StyleResolver::MatchedPropertiesCacheItem* StyleResolver::findFromMatchedPropertiesCache(unsigned hash, const MatchResult& matchResult)
{
    ASSERT(hash);

    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.find(hash);
    if (it == m_matchedPropertiesCache.end())
        return 0;
    MatchedPropertiesCacheItem& cacheItem = it->value;

    size_t size = matchResult.matchedProperties.size();
    if (size != cacheItem.matchedProperties.size())
        return 0;
    for (size_t i = 0; i < size; ++i) {
        if (matchResult.matchedProperties[i] != cacheItem.matchedProperties[i])
            return 0;
    }
    if (cacheItem.ranges != matchResult.ranges)
        return 0;
    return &cacheItem;
}

void StyleResolver::addToMatchedPropertiesCache(const RenderStyle* style, const RenderStyle* parentStyle, unsigned hash, const MatchResult& matchResult)
{
    static const unsigned matchedDeclarationCacheAdditionsBetweenSweeps = 100;
    if (++m_matchedPropertiesCacheAdditionsSinceLastSweep >= matchedDeclarationCacheAdditionsBetweenSweeps
        && !m_matchedPropertiesCacheSweepTimer.isActive()) {
        static const unsigned matchedDeclarationCacheSweepTimeInSeconds = 60;
        m_matchedPropertiesCacheSweepTimer.startOneShot(matchedDeclarationCacheSweepTimeInSeconds);
    }

    ASSERT(hash);
    MatchedPropertiesCacheItem cacheItem;
    cacheItem.matchedProperties.appendVector(matchResult.matchedProperties);
    cacheItem.ranges = matchResult.ranges;
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    cacheItem.renderStyle = RenderStyle::clone(style);
    cacheItem.parentRenderStyle = RenderStyle::clone(parentStyle);
    m_matchedPropertiesCache.add(hash, std::move(cacheItem));
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

static bool isCacheableInMatchedPropertiesCache(const Element* element, const RenderStyle* style, const RenderStyle* parentStyle)
{
    // FIXME: CSSPropertyWebkitWritingMode modifies state when applying to document element. We can't skip the applying by caching.
    if (element == element->document().documentElement() && element->document().writingModeSetOnDocumentElement())
        return false;
    if (style->unique() || (style->styleType() != NOPSEUDO && parentStyle->unique()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    if (style->writingMode() != RenderStyle::initialWritingMode())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void extractDirectionAndWritingMode(const RenderStyle& style, const StyleResolver::MatchResult& matchResult, TextDirection& direction, WritingMode& writingMode)
{
    direction = style.direction();
    writingMode = style.writingMode();

    bool hadImportantWebkitWritingMode = false;
    bool hadImportantDirection = false;

    for (auto& matchedProperties : matchResult.matchedProperties) {
        for (unsigned i = 0, count = matchedProperties.properties->propertyCount(); i < count; ++i) {
            auto property = matchedProperties.properties->propertyAt(i);
            if (!property.value()->isPrimitiveValue())
                continue;
            switch (property.id()) {
            case CSSPropertyWebkitWritingMode:
                if (!hadImportantWebkitWritingMode || property.isImportant()) {
                    writingMode = toCSSPrimitiveValue(*property.value());
                    hadImportantWebkitWritingMode = property.isImportant();
                }
                break;
            case CSSPropertyDirection:
                if (!hadImportantDirection || property.isImportant()) {
                    direction = toCSSPrimitiveValue(*property.value());
                    hadImportantDirection = property.isImportant();
                }
                break;
            default:
                break;
            }
        }
    }
}

void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, const Element* element, ShouldUseMatchedPropertiesCache shouldUseMatchedPropertiesCache)
{
    ASSERT(element);
    State& state = m_state;
    unsigned cacheHash = shouldUseMatchedPropertiesCache && matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const MatchedPropertiesCacheItem* cacheItem = 0;
    if (cacheHash && (cacheItem = findFromMatchedPropertiesCache(cacheHash, matchResult))) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the 
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cacheItem->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cacheItem->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cacheItem->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true; 
    }

    // Directional properties (*-before/after) are aliases that depend on the TextDirection and WritingMode.
    // These must be resolved before we can begin the property cascade.
    TextDirection direction;
    WritingMode writingMode;
    extractDirectionAndWritingMode(*state.style(), matchResult, direction, writingMode);

    if (elementTypeHasAppearanceFromUAStyle(*state.element())) {
        // FIXME: This is such a hack.
        // Find out if there's a -webkit-appearance property in effect from the UA sheet.
        // If so, we cache the border and background styles so that RenderTheme::adjustStyle()
        // can look at them later to figure out if this is a styled form control or not.
        state.setLineHeightValue(nullptr);
        CascadedProperties cascade(direction, writingMode);
        if (!cascade.addMatches(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly)
            || !cascade.addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly))
            return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

        applyCascadedProperties(cascade, firstCSSProperty, CSSPropertyLineHeight);
        updateFont();
        applyCascadedProperties(cascade, CSSPropertyBackground, lastCSSProperty);

        state.cacheBorderAndBackground();
    }

    CascadedProperties cascade(direction, writingMode);
    if (!cascade.addMatches(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly)
        || !cascade.addMatches(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly)
        || !cascade.addMatches(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly)
        || !cascade.addMatches(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly))
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    state.setLineHeightValue(nullptr);

    // Start by applying properties that other properties may depend on.
    applyCascadedProperties(cascade, firstCSSProperty, CSSPropertyLineHeight);

    // If the effective zoom value changes, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->effectiveZoom() != state.style()->effectiveZoom())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // If the font changed, we can't use the matched properties cache. Start over.
    if (cacheItem && cacheItem->renderStyle->fontDescription() != state.style()->fontDescription())
        return applyMatchedProperties(matchResult, element, DoNotUseMatchedPropertiesCache);

    // Apply properties that no other properties depend on.
    applyCascadedProperties(cascade, CSSPropertyBackground, lastCSSProperty);

    // Finally, some properties must be applied in the order they were parsed.
    // There are some CSS properties that affect the same RenderStyle values,
    // so to preserve behavior, we queue them up during cascade and flush here.
    cascade.applyDeferredProperties(*this);

    // Start loading resources referenced by this style.
    loadPendingResources();
    
    ASSERT(!state.fontDirty());
    
    if (cacheItem || !cacheHash)
        return;
    if (!isCacheableInMatchedPropertiesCache(state.element(), state.style(), state.parentStyle()))
        return;
    addToMatchedPropertiesCache(state.style(), state.parentStyle(), cacheHash, matchResult);
}

void StyleResolver::applyPropertyToStyle(CSSPropertyID id, CSSValue* value, RenderStyle* style)
{
    initElement(0);
    m_state.initForStyleResolve(document(), 0, style);
    m_state.setStyle(*style);
    applyPropertyToCurrentStyle(id, value);
}

void StyleResolver::applyPropertyToCurrentStyle(CSSPropertyID id, CSSValue* value)
{
    if (value)
        applyProperty(id, value);
}

inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyFill:
    case CSSPropertyStroke:
        return true;
    default:
        break;
    }

    return false;
}

// http://dev.w3.org/csswg/css3-regions/#the-at-region-style-rule
// FIXME: add incremental support for other region styling properties.
inline bool StyleResolver::isValidRegionStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyColor:
        return true;
    default:
        break;
    }

    return false;
}

#if ENABLE(VIDEO_TRACK)
inline bool StyleResolver::isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    default:
        break;
    }
    return false;
}
#endif
// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool StyleResolver::useSVGZoomRules()
{
    return m_state.element() && m_state.element()->isSVGElement();
}

#if ENABLE(CSS_GRID_LAYOUT)
static bool createGridTrackBreadth(CSSPrimitiveValue* primitiveValue, const StyleResolver::State& state, GridLength& workingLength)
{
    if (primitiveValue->getValueID() == CSSValueWebkitMinContent) {
        workingLength = Length(MinContent);
        return true;
    }

    if (primitiveValue->getValueID() == CSSValueWebkitMaxContent) {
        workingLength = Length(MaxContent);
        return true;
    }

    if (primitiveValue->isFlex()) {
        // Fractional unit.
        workingLength.setFlex(primitiveValue->getDoubleValue());
        return true;
    }

    workingLength = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | ViewportPercentageConversion | CalculatedConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    if (workingLength.length().isUndefined())
        return false;

    if (primitiveValue->isLength())
        workingLength.length().setQuirk(primitiveValue->isQuirkValue());

    return true;
}

static bool createGridTrackSize(CSSValue* value, GridTrackSize& trackSize, const StyleResolver::State& state)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        GridLength workingLength;
        if (!createGridTrackBreadth(primitiveValue, state, workingLength))
            return false;

        trackSize.setLength(workingLength);
        return true;
    }

    CSSFunctionValue* minmaxFunction = toCSSFunctionValue(value);
    CSSValueList* arguments = minmaxFunction->arguments();
    ASSERT_WITH_SECURITY_IMPLICATION(arguments->length() == 2);
    GridLength minTrackBreadth;
    GridLength maxTrackBreadth;
    if (!createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(0)), state, minTrackBreadth) || !createGridTrackBreadth(toCSSPrimitiveValue(arguments->itemWithoutBoundsCheck(1)), state, maxTrackBreadth))
        return false;

    trackSize.setMinMax(minTrackBreadth, maxTrackBreadth);
    return true;
}

static bool createGridTrackList(CSSValue* value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, const StyleResolver::State& state)
{
    // Handle 'none'.
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->getValueID() == CSSValueNone;
    }

    if (!value->isValueList())
        return false;

    size_t currentNamedGridLine = 0;
    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (currValue->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(currValue);
            if (primitiveValue->isString()) {
                NamedGridLinesMap::AddResult result = namedGridLines.add(primitiveValue->getStringValue(), Vector<size_t>());
                result.iterator->value.append(currentNamedGridLine);
                continue;
            }
        }

        ++currentNamedGridLine;
        GridTrackSize trackSize;
        if (!createGridTrackSize(currValue, trackSize, state))
            return false;

        trackSizes.append(trackSize);
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty());
    return true;
}


static bool createGridPosition(CSSValue* value, GridPosition& position)
{
    // We accept the specification's grammar:
    // 'auto' | [ <integer> || <string> ] | [ span && [ <integer> || string ] ] | <ident>
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        // We translate <ident> to <string> during parsing as it makes handling it simpler.
        if (primitiveValue->isString()) {
            position.setNamedGridArea(primitiveValue->getStringValue());
            return true;
        }

        ASSERT(primitiveValue->getValueID() == CSSValueAuto);
        return true;
    }

    CSSValueList* values = toCSSValueList(value);
    ASSERT(values->length());

    bool isSpanPosition = false;
    // The specification makes the <integer> optional, in which case it default to '1'.
    int gridLineNumber = 1;
    String gridLineName;

    CSSValueListIterator it = values;
    CSSPrimitiveValue* currentValue = toCSSPrimitiveValue(it.value());
    if (currentValue->getValueID() == CSSValueSpan) {
        isSpanPosition = true;
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isNumber()) {
        gridLineNumber = currentValue->getIntValue();
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isString()) {
        gridLineName = currentValue->getStringValue();
        it.advance();
    }

    ASSERT(!it.hasMore());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return true;
}
#endif /* ENABLE(CSS_GRID_LAYOUT) */

void StyleResolver::applyProperty(CSSPropertyID id, CSSValue* value)
{
    ASSERT_WITH_MESSAGE(!isExpandedShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    State& state = m_state;

    if (CSSProperty::isDirectionAwareProperty(id)) {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, state.style()->direction(), state.style()->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, value);
    }

    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    // Check lookup table for implementations and use when available.
    const PropertyHandler& handler = m_deprecatedStyleBuilder.propertyHandler(id);
    if (handler.isValid()) {
        if (isInherit)
            handler.applyInheritValue(id, this);
        else if (isInitial)
            handler.applyInitialValue(id, this);
        else
            handler.applyValue(id, this, value);
        return;
    }

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;

    float zoomFactor = state.style()->effectiveZoom();

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.
    switch (id) {
    // lists
    case CSSPropertyContent:
        // list of string, uri, counter, attr, i
        {
            // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
            // note is a reminder that eventually "inherit" needs to be supported.

            if (isInitial) {
                state.style()->clearContent();
                return;
            }

            if (!value->isValueList())
                return;

            bool didSet = false;
            for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
                CSSValue* item = i.value();
                if (item->isImageGeneratorValue()) {
                    if (item->isGradientValue())
                        state.style()->setContent(StyleGeneratedImage::create(*toCSSGradientValue(item)->gradientWithStylesResolved(this)), didSet);
                    else
                        state.style()->setContent(StyleGeneratedImage::create(*toCSSImageGeneratorValue(item)), didSet);
                    didSet = true;
#if ENABLE(CSS_IMAGE_SET)
                } else if (item->isImageSetValue()) {
                    state.style()->setContent(setOrPendingFromValue(CSSPropertyContent, toCSSImageSetValue(item)), didSet);
                    didSet = true;
#endif
                }

                if (item->isImageValue()) {
                    state.style()->setContent(cachedOrPendingFromValue(CSSPropertyContent, toCSSImageValue(item)), didSet);
                    didSet = true;
                    continue;
                }

                if (!item->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);

                if (contentValue->isString()) {
                    state.style()->setContent(contentValue->getStringValue().impl(), didSet);
                    didSet = true;
                } else if (contentValue->isAttr()) {
                    // FIXME: Can a namespace be specified for an attr(foo)?
                    if (state.style()->styleType() == NOPSEUDO)
                        state.style()->setUnique();
                    else
                        state.parentStyle()->setUnique();
                    QualifiedName attr(nullAtom, contentValue->getStringValue().impl(), nullAtom);
                    const AtomicString& value = state.element()->getAttribute(attr);
                    state.style()->setContent(value.isNull() ? emptyAtom : value.impl(), didSet);
                    didSet = true;
                    // Register the fact that the attribute value affects the style.
                    m_ruleSets.features().attrsInRules.add(attr.localName().impl());
                } else if (contentValue->isCounter()) {
                    Counter* counterValue = contentValue->getCounterValue();
                    EListStyleType listStyleType = NoneListStyle;
                    CSSValueID listStyleIdent = counterValue->listStyleIdent();
                    if (listStyleIdent != CSSValueNone)
                        listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
                    auto counter = std::make_unique<CounterContent>(counterValue->identifier(), listStyleType, counterValue->separator());
                    state.style()->setContent(std::move(counter), didSet);
                    didSet = true;
                } else {
                    switch (contentValue->getValueID()) {
                    case CSSValueOpenQuote:
                        state.style()->setContent(OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueCloseQuote:
                        state.style()->setContent(CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoOpenQuote:
                        state.style()->setContent(NO_OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoCloseQuote:
                        state.style()->setContent(NO_CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    default:
                        // normal and none do not have any effect.
                        { }
                    }
                }
            }
            if (!didSet)
                state.style()->clearContent();
            return;
        }
    case CSSPropertyWebkitAlt:
        {
            bool didSet = false;
            if (primitiveValue->isString()) {
                state.style()->setContentAltText(primitiveValue->getStringValue().impl());
                didSet = true;
            } else if (primitiveValue->isAttr()) {
                // FIXME: Can a namespace be specified for an attr(foo)?
                if (state.style()->styleType() == NOPSEUDO)
                    state.style()->setUnique();
                else
                    state.parentStyle()->setUnique();
                QualifiedName attr(nullAtom, primitiveValue->getStringValue().impl(), nullAtom);
                const AtomicString& value = state.element()->getAttribute(attr);
                state.style()->setContentAltText(value.isNull() ? emptyAtom : value.impl());
                didSet = true;
                // Register the fact that the attribute value affects the style.
                m_ruleSets.features().attrsInRules.add(attr.localName().impl());
            }
            if (!didSet)
                state.style()->setContentAltText(emptyAtom);
            return;
        }
        
    case CSSPropertyQuotes:
        if (isInherit) {
            state.style()->setQuotes(state.parentStyle()->quotes());
            return;
        }
        if (isInitial) {
            state.style()->setQuotes(0);
            return;
        }
        if (value->isValueList()) {
            CSSValueList* list = toCSSValueList(value);
            Vector<std::pair<String, String>> quotes;
            for (size_t i = 0; i < list->length(); i += 2) {
                CSSValue* first = list->itemWithoutBoundsCheck(i);
                // item() returns null if out of bounds so this is safe.
                CSSValue* second = list->item(i + 1);
                if (!second)
                    continue;
                ASSERT_WITH_SECURITY_IMPLICATION(first->isPrimitiveValue());
                ASSERT_WITH_SECURITY_IMPLICATION(second->isPrimitiveValue());
                String startQuote = toCSSPrimitiveValue(first)->getStringValue();
                String endQuote = toCSSPrimitiveValue(second)->getStringValue();
                quotes.append(std::make_pair(startQuote, endQuote));
            }
            state.style()->setQuotes(QuotesData::create(quotes));
            return;
        }
        if (primitiveValue) {
            if (primitiveValue->getValueID() == CSSValueNone)
                state.style()->setQuotes(QuotesData::create(Vector<std::pair<String, String>>()));
        }
        return;
    // Shorthand properties.
    case CSSPropertyFont:
        if (isInherit) {
            FontDescription fontDescription = state.parentStyle()->fontDescription();
            state.style()->setLineHeight(state.parentStyle()->specifiedLineHeight());
            state.setLineHeightValue(0);
            setFontDescription(fontDescription);
        } else if (isInitial) {
            Settings* settings = documentSettings();
            ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
            if (!settings)
                return;
            initializeFontStyle(settings);
        } else if (primitiveValue) {
            state.style()->setLineHeight(RenderStyle::initialLineHeight());
            state.setLineHeightValue(0);

            FontDescription fontDescription;
            RenderTheme::defaultTheme()->systemFont(primitiveValue->getValueID(), fontDescription);

            // Double-check and see if the theme did anything. If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Make sure the rendering mode and printer font settings are updated.
                Settings* settings = documentSettings();
                ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
                if (!settings)
                    return;
                fontDescription.setRenderingMode(settings->fontRenderingMode());
                fontDescription.setUsePrinterFont(document().printing() || !settings->screenFontSubstitutionEnabled());

                // Handle the zoom factor.
                fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(fontDescription.specifiedSize(), fontDescription.isAbsoluteSize(), useSVGZoomRules(), state.style(), document()));
                setFontDescription(fontDescription);
            }
        } else if (value->isFontValue()) {
            CSSFontValue* font = toCSSFontValue(value);
            if (!font->style || !font->variant || !font->weight
                || !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSSPropertyFontStyle, font->style.get());
            applyProperty(CSSPropertyFontVariant, font->variant.get());
            applyProperty(CSSPropertyFontWeight, font->weight.get());
            // The previous properties can dirty our font but they don't try to read the font's
            // properties back, which is safe. However if font-size is using the 'ex' unit, it will
            // need query the dirtied font's x-height to get the computed size. To be safe in this
            // case, let's just update the font now.
            updateFont();
            applyProperty(CSSPropertyFontSize, font->size.get());

            state.setLineHeightValue(font->lineHeight.get());

            applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;

    case CSSPropertyBackground:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBorder:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRadius:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderSpacing:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderWidth:
    case CSSPropertyListStyle:
    case CSSPropertyMargin:
    case CSSPropertyOutline:
    case CSSPropertyOverflow:
    case CSSPropertyPadding:
    case CSSPropertyTransition:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitColumns:
    case CSSPropertyWebkitColumnRule:
    case CSSPropertyWebkitFlex:
    case CSSPropertyWebkitFlexFlow:
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridArea:
    case CSSPropertyWebkitGridColumn:
    case CSSPropertyWebkitGridRow:
#endif
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarquee:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransformOrigin:
        ASSERT(isExpandedShorthand(id));
        ASSERT_NOT_REACHED();
        break;

    // CSS3 Properties
    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return state.style()->setTextShadow(state.parentStyle()->textShadow() ? std::make_unique<ShadowData>(*state.parentStyle()->textShadow()) : nullptr);
            return state.style()->setBoxShadow(state.parentStyle()->boxShadow() ? std::make_unique<ShadowData>(*state.parentStyle()->boxShadow()) : nullptr);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? state.style()->setTextShadow(nullptr) : state.style()->setBoxShadow(nullptr);

        if (!value->isValueList())
            return;

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* currValue = i.value();
            if (!currValue->isShadowValue())
                continue;
            CSSShadowValue* item = toCSSShadowValue(currValue);
            int x = item->x->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            if (item->x->isViewportPercentageLength())
                x = viewportPercentageValue(*item->x, x);
            int y = item->y->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            if (item->y->isViewportPercentageLength())
                y = viewportPercentageValue(*item->y, y);
            int blur = item->blur ? item->blur->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            if (item->blur && item->blur->isViewportPercentageLength())
                blur = viewportPercentageValue(*item->blur, blur);
            int spread = item->spread ? item->spread->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            if (item->spread && item->spread->isViewportPercentageLength())
                spread = viewportPercentageValue(*item->spread, spread);
            ShadowStyle shadowStyle = item->style && item->style->getValueID() == CSSValueInset ? Inset : Normal;
            Color color;
            if (item->color)
                color = colorFromPrimitiveValue(item->color.get());
            else if (state.style())
                color = state.style()->color();

            auto shadowData = std::make_unique<ShadowData>(IntPoint(x, y), blur, spread, shadowStyle, id == CSSPropertyWebkitBoxShadow, color.isValid() ? color : Color::transparent);
            if (id == CSSPropertyTextShadow)
                state.style()->setTextShadow(std::move(shadowData), i.index()); // add to the list if this is not the first entry
            else
                state.style()->setBoxShadow(std::move(shadowData), i.index()); // add to the list if this is not the first entry
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            state.style()->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }

        if (!value->isReflectValue())
            return;

        CSSReflectValue* reflectValue = toCSSReflectValue(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(*reflectValue->direction());
        if (reflectValue->offset())
            reflection->setOffset(reflectValue->offset()->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(state.style(), state.rootElementStyle(), zoomFactor));
        NinePieceImage mask;
        mask.setMaskDefaults();
        m_styleMap.mapNinePieceImage(id, reflectValue->mask(), mask);
        reflection->setMask(mask);

        state.style()->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitLocale: {
        HANDLE_INHERIT_AND_INITIAL(locale, Locale);
        if (!primitiveValue)
            return;
        if (primitiveValue->getValueID() == CSSValueAuto)
            state.style()->setLocale(nullAtom);
        else
            state.style()->setLocale(primitiveValue->getStringValue());
        FontDescription fontDescription = state.style()->fontDescription();
        fontDescription.setScript(localeToScriptCodeForFontSelection(state.style()->locale()));
        setFontDescription(fontDescription);
        return;
    }
#if ENABLE(IOS_TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue)
            return;

        if (primitiveValue->getValueID() == CSSValueAuto)
            state.style()->setTextSizeAdjust(TextSizeAdjustment(AutoTextSizeAdjustment));
        else if (primitiveValue->getValueID() == CSSValueNone)
            state.style()->setTextSizeAdjust(TextSizeAdjustment(NoTextSizeAdjustment));
        else
            state.style()->setTextSizeAdjust(TextSizeAdjustment(primitiveValue->getFloatValue()));

        state.setFontDirty(true);
        return;
    }
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion:
    {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegion* region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;

        DashboardRegion* first = region;
        while (region) {
            Length top = convertToIntLength(region->top(), state.style(), state.rootElementStyle());
            Length right = convertToIntLength(region->right(), state.style(), state.rootElementStyle());
            Length bottom = convertToIntLength(region->bottom(), state.style(), state.rootElementStyle());
            Length left = convertToIntLength(region->left(), state.style(), state.rootElementStyle());

            if (top.isUndefined())
                top = Length();
            if (right.isUndefined())
                right = Length();
            if (bottom.isUndefined())
                bottom = Length();
            if (left.isUndefined())
                left = Length();

            if (region->m_isCircle)
                state.style()->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            else if (region->m_isRectangle)
                state.style()->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            region = region->m_next.get();
        }

        state.document().setHasAnnotatedRegions(true);

        return;
    }
#endif
    case CSSPropertyWebkitTextStrokeWidth: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getValueID()) {
        case CSSValueThin:
        case CSSValueMedium:
        case CSSValueThick: {
            double result = 1.0 / 48;
            if (primitiveValue->getValueID() == CSSValueMedium)
                result *= 3;
            else if (primitiveValue->getValueID() == CSSValueThick)
                result *= 5;
            Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS));
            width = value.get().computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        default:
            width = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        state.style()->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        transformsForValue(state.style(), state.rootElementStyle(), value, operations);
        state.style()->setTransform(operations);
        return;
    }
    case CSSPropertyWebkitPerspective: {
        HANDLE_INHERIT_AND_INITIAL(perspective, Perspective)

        if (!primitiveValue)
            return;

        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setPerspective(0);
            return;
        }

        float perspectiveValue;
        if (primitiveValue->isLength())
            perspectiveValue = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        else if (primitiveValue->isNumber()) {
            // For backward compatibility, treat valueless numbers as px.
            Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX));
            perspectiveValue = value.get().computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        } else
            return;

        if (perspectiveValue >= 0.0f)
            state.style()->setPerspective(perspectiveValue);
        return;
    }
#if PLATFORM(IOS)
    case CSSPropertyWebkitTouchCallout: {
        HANDLE_INHERIT_AND_INITIAL(touchCalloutEnabled, TouchCalloutEnabled);
        if (!primitiveValue)
            break;

        state.style()->setTouchCalloutEnabled(primitiveValue->getStringValue().lower() != "none");
        return;
    }

    // FIXME: CSSPropertyWebkitCompositionFillColor shouldn't be iOS-specific. Once we fix up its usage in
    // InlineTextBox::paintCompositionBackground() we should move it outside the PLATFORM(IOS)-guard.
    // See <https://bugs.webkit.org/show_bug.cgi?id=126296>.
    case CSSPropertyWebkitCompositionFillColor: {
        HANDLE_INHERIT_AND_INITIAL(compositionFillColor, CompositionFillColor);
        if (!primitiveValue)
            break;
        state.style()->setCompositionFillColor(colorFromPrimitiveValue(primitiveValue));
        return;
    }
#endif
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor: {
        HANDLE_INHERIT_AND_INITIAL(tapHighlightColor, TapHighlightColor);
        if (!primitiveValue)
            break;

        Color col = colorFromPrimitiveValue(primitiveValue);
        state.style()->setTapHighlightColor(col);
        return;
    }
#endif
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    case CSSPropertyWebkitOverflowScrolling: {
        HANDLE_INHERIT_AND_INITIAL(useTouchOverflowScrolling, UseTouchOverflowScrolling);
        if (!primitiveValue)
            break;
        state.style()->setUseTouchOverflowScrolling(primitiveValue->getValueID() == CSSValueTouch);
        return;
    }
#endif
    case CSSPropertyInvalid:
        return;
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverline:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderline:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitTextDecorationsInEffect:
        return;

    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitWritingMode: {
        HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode);
        
        if (primitiveValue)
            setWritingMode(*primitiveValue);

        // FIXME: It is not ok to modify document state while applying style.
        if (state.element() && state.element() == state.document().documentElement())
            state.document().setWritingModeSetOnDocumentElement(true);
        return;
    }

    case CSSPropertyWebkitTextOrientation: {
        HANDLE_INHERIT_AND_INITIAL(textOrientation, TextOrientation);

        if (primitiveValue)
            setTextOrientation(*primitiveValue);

        return;
    }

    case CSSPropertyWebkitLineBoxContain: {
        HANDLE_INHERIT_AND_INITIAL(lineBoxContain, LineBoxContain)
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setLineBoxContain(LineBoxContainNone);
            return;
        }

        if (!value->isLineBoxContainValue())
            return;

        state.style()->setLineBoxContain(toCSSLineBoxContainValue(value)->value());
        return;
    }

    // CSS Fonts Module Level 3
    case CSSPropertyWebkitFontFeatureSettings: {
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNormal) {
            setFontDescription(state.style()->fontDescription().makeNormalFeatureSettings());
            return;
        }

        if (!value->isValueList())
            return;

        FontDescription fontDescription = state.style()->fontDescription();
        CSSValueList* list = toCSSValueList(value);
        RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
        int len = list->length();
        for (int i = 0; i < len; ++i) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isFontFeatureValue())
                continue;
            CSSFontFeatureValue* feature = toCSSFontFeatureValue(item);
            settings->append(FontFeature(feature->tag(), feature->value()));
        }
        fontDescription.setFeatureSettings(settings.release());
        setFontDescription(fontDescription);
        return;
    }

#if ENABLE(CSS_FILTERS)
    case CSSPropertyWebkitFilter: {
        HANDLE_INHERIT_AND_INITIAL(filter, Filter);
        FilterOperations operations;
        if (createFilterOperations(value, operations))
            state.style()->setFilter(operations);
        return;
    }
#endif
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridAutoColumns: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoColumns, GridAutoColumns);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoColumns(trackSize);
        return;
    }
    case CSSPropertyWebkitGridAutoRows: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoRows, GridAutoRows);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoRows(trackSize);
        return;
    }
    case CSSPropertyWebkitGridTemplateColumns: {
        if (isInherit) {
            m_state.style()->setGridColumns(m_state.parentStyle()->gridColumns());
            m_state.style()->setNamedGridColumnLines(m_state.parentStyle()->namedGridColumnLines());
            return;
        }
        if (isInitial) {
            m_state.style()->setGridColumns(RenderStyle::initialGridColumns());
            m_state.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
            return;
        }
        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridColumns(trackSizes);
        state.style()->setNamedGridColumnLines(namedGridLines);
        return;
    }
    case CSSPropertyWebkitGridTemplateRows: {
        if (isInherit) {
            m_state.style()->setGridRows(m_state.parentStyle()->gridRows());
            m_state.style()->setNamedGridRowLines(m_state.parentStyle()->namedGridRowLines());
            return;
        }
        if (isInitial) {
            m_state.style()->setGridRows(RenderStyle::initialGridRows());
            m_state.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
            return;
        }
        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridRows(trackSizes);
        state.style()->setNamedGridRowLines(namedGridLines);
        return;
    }

    case CSSPropertyWebkitGridColumnStart: {
        HANDLE_INHERIT_AND_INITIAL(gridItemColumnStart, GridItemColumnStart);
        GridPosition columnStartPosition;
        if (!createGridPosition(value, columnStartPosition))
            return;
        state.style()->setGridItemColumnStart(columnStartPosition);
        return;
    }
    case CSSPropertyWebkitGridColumnEnd: {
        HANDLE_INHERIT_AND_INITIAL(gridItemColumnEnd, GridItemColumnEnd);
        GridPosition columnEndPosition;
        if (!createGridPosition(value, columnEndPosition))
            return;
        state.style()->setGridItemColumnEnd(columnEndPosition);
        return;
    }

    case CSSPropertyWebkitGridRowStart: {
        HANDLE_INHERIT_AND_INITIAL(gridItemRowStart, GridItemRowStart);
        GridPosition rowStartPosition;
        if (!createGridPosition(value, rowStartPosition))
            return;
        state.style()->setGridItemRowStart(rowStartPosition);
        return;
    }
    case CSSPropertyWebkitGridRowEnd: {
        HANDLE_INHERIT_AND_INITIAL(gridItemRowEnd, GridItemRowEnd);
        GridPosition rowEndPosition;
        if (!createGridPosition(value, rowEndPosition))
            return;
        state.style()->setGridItemRowEnd(rowEndPosition);
        return;
    }
    case CSSPropertyWebkitGridTemplateAreas: {
        if (isInherit) {
            state.style()->setNamedGridArea(state.parentStyle()->namedGridArea());
            state.style()->setNamedGridAreaRowCount(state.parentStyle()->namedGridAreaRowCount());
            state.style()->setNamedGridAreaColumnCount(state.parentStyle()->namedGridAreaColumnCount());
            return;
        }
        if (isInitial) {
            state.style()->setNamedGridArea(RenderStyle::initialNamedGridArea());
            state.style()->setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
            state.style()->setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
            return;
        }

        if (value->isPrimitiveValue()) {
            ASSERT(toCSSPrimitiveValue(value)->getValueID() == CSSValueNone);
            return;
        }

        CSSGridTemplateAreasValue* gridTemplateValue = toCSSGridTemplateAreasValue(value);
        state.style()->setNamedGridArea(gridTemplateValue->gridAreaMap());
        state.style()->setNamedGridAreaRowCount(gridTemplateValue->rowCount());
        state.style()->setNamedGridAreaColumnCount(gridTemplateValue->columnCount());
        return;
    }
#endif /* ENABLE(CSS_GRID_LAYOUT) */
    // These properties are aliased and DeprecatedStyleBuilder already applied the property on the prefixed version.
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
        return;
    // These properties are implemented in the DeprecatedStyleBuilder lookup table.
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxSizing:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyClip:
    case CSSPropertyColor:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyCursor:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyHeight:
#if ENABLE(CSS_IMAGE_ORIENTATION)
    case CSSPropertyImageOrientation:
#endif
    case CSSPropertyImageRendering:
#if ENABLE(CSS_IMAGE_RESOLUTION)
    case CSSPropertyImageResolution:
#endif
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyObjectFit:
    case CSSPropertyOpacity:
    case CSSPropertyOrphans:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyRight:
    case CSSPropertySize:
    case CSSPropertySpeak:
    case CSSPropertyTabSize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextIndent:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTop:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVerticalAlign:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxAlign:
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
#endif
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxFlex:
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColorCorrection:
    case CSSPropertyWebkitColumnAxis:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnProgression:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
#endif
    case CSSPropertyWebkitAlignContent:
    case CSSPropertyWebkitAlignItems:
    case CSSPropertyWebkitAlignSelf:
    case CSSPropertyWebkitFlexBasis:
    case CSSPropertyWebkitFlexDirection:
    case CSSPropertyWebkitFlexGrow:
    case CSSPropertyWebkitFlexShrink:
    case CSSPropertyWebkitFlexWrap:
    case CSSPropertyWebkitJustifyContent:
    case CSSPropertyWebkitOrder:
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitFlowFrom:
    case CSSPropertyWebkitFlowInto:
#endif
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitFontVariantLigatures:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitHyphenateLimitAfter:
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitLines:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeSpeed:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskSourceType:
    case CSSPropertyWebkitNbspMode:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPrintColorAdjust:
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitRegionFragment:
#endif
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
    case CSSPropertyWebkitTextJustify:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextDecorationLine:
    case CSSPropertyWebkitTextDecorationStyle:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextDecorationSkip:
    case CSSPropertyWebkitTextUnderlinePosition:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitClipPath:
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    case CSSPropertyWebkitShapeInside:
    case CSSPropertyWebkitShapePadding:
#endif
#if ENABLE(CSS_SHAPES)
    case CSSPropertyWebkitShapeMargin:
    case CSSPropertyWebkitShapeImageThreshold:
    case CSSPropertyWebkitShapeOutside:
#endif
#if ENABLE(CSS_EXCLUSIONS)
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitWrapThrough:
#endif
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWordWrap:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
#endif
        ASSERT_NOT_REACHED();
        return;
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
        return;
    }
}

PassRefPtr<StyleImage> StyleResolver::styleImage(CSSPropertyID property, CSSValue* value)
{
    if (value->isImageValue())
        return cachedOrPendingFromValue(property, toCSSImageValue(value));

    if (value->isImageGeneratorValue()) {
        if (value->isGradientValue())
            return generatedOrPendingFromValue(property, *toCSSGradientValue(value)->gradientWithStylesResolved(this));
        return generatedOrPendingFromValue(property, toCSSImageGeneratorValue(*value));
    }

#if ENABLE(CSS_IMAGE_SET)
    if (value->isImageSetValue())
        return setOrPendingFromValue(property, toCSSImageSetValue(value));
#endif

    if (value->isCursorImageValue())
        return cursorOrPendingFromValue(property, toCSSCursorImageValue(value));

    return 0;
}

PassRefPtr<StyleImage> StyleResolver::cachedOrPendingFromValue(CSSPropertyID property, CSSImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage();
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}

PassRefPtr<StyleImage> StyleResolver::generatedOrPendingFromValue(CSSPropertyID property, CSSImageGeneratorValue& value)
{
#if ENABLE(CSS_FILTERS)
    if (value.isFilterImageValue()) {
        // FilterImage needs to calculate FilterOperations.
        toCSSFilterImageValue(value).createFilterOperations(this);
    }
#endif
    if (value.isPending()) {
        m_state.pendingImageProperties().set(property, &value);
        return StylePendingImage::create(&value);
    }
    return StyleGeneratedImage::create(value);
}

#if ENABLE(CSS_IMAGE_SET)
PassRefPtr<StyleImage> StyleResolver::setOrPendingFromValue(CSSPropertyID property, CSSImageSetValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImageSet(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}
#endif

PassRefPtr<StyleImage> StyleResolver::cursorOrPendingFromValue(CSSPropertyID property, CSSCursorImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
void StyleResolver::checkForTextSizeAdjust(RenderStyle* style)
{
    if (style->textSizeAdjust().isAuto())
        return;

    FontDescription newFontDescription(style->fontDescription());
    if (!style->textSizeAdjust().isNone())
        newFontDescription.setComputedSize(newFontDescription.specifiedSize() * style->textSizeAdjust().multiplier());
    else
        newFontDescription.setComputedSize(newFontDescription.specifiedSize());
    style->setFontDescription(newFontDescription);
}
#endif

void StyleResolver::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (!parentStyle)
        return;
    
    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;

    const FontDescription& childFont = style->fontDescription();
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void StyleResolver::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();

    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily
        && parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (childFont.keywordSize())
        size = Style::fontSizeForKeyword(CSSValueXxSmall + childFont.keywordSize() - 1, childFont.useFixedDefaultSize(), document());
    else {
        Settings* settings = documentSettings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
                childFont.specifiedSize() / fixedScaleFactor :
                childFont.specifiedSize() * fixedScaleFactor;
    }

    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::initializeFontStyle(Settings* settings)
{
    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::StandardFamily);
    fontDescription.setRenderingMode(settings->fontRenderingMode());
    fontDescription.setUsePrinterFont(document().printing() || !settings->screenFontSubstitutionEnabled());
    const AtomicString& standardFontFamily = documentSettings()->standardFontFamily();
    if (!standardFontFamily.isEmpty())
        fontDescription.setOneFamily(standardFontFamily);
    fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    setFontSize(fontDescription, Style::fontSizeForKeyword(CSSValueMedium, false, document()));
    m_state.style()->setLineHeight(RenderStyle::initialLineHeight());
    m_state.setLineHeightValue(0);
    setFontDescription(fontDescription);
}

void StyleResolver::setFontSize(FontDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), m_state.style(), document()));
}

static Color colorForCSSValue(CSSValueID cssValueId)
{
    struct ColorValue {
        CSSValueID cssValueId;
        RGBA32 color;
    };

    static const ColorValue colorValues[] = {
        { CSSValueAqua, 0xFF00FFFF },
        { CSSValueBlack, 0xFF000000 },
        { CSSValueBlue, 0xFF0000FF },
        { CSSValueFuchsia, 0xFFFF00FF },
        { CSSValueGray, 0xFF808080 },
        { CSSValueGreen, 0xFF008000  },
        { CSSValueGrey, 0xFF808080 },
        { CSSValueLime, 0xFF00FF00 },
        { CSSValueMaroon, 0xFF800000 },
        { CSSValueNavy, 0xFF000080 },
        { CSSValueOlive, 0xFF808000  },
        { CSSValueOrange, 0xFFFFA500 },
        { CSSValuePurple, 0xFF800080 },
        { CSSValueRed, 0xFFFF0000 },
        { CSSValueSilver, 0xFFC0C0C0 },
        { CSSValueTeal, 0xFF008080  },
        { CSSValueTransparent, 0x00000000 },
        { CSSValueWhite, 0xFFFFFFFF },
        { CSSValueYellow, 0xFFFFFF00 },
        { CSSValueInvalid, CSSValueInvalid }
    };

    for (const ColorValue* col = colorValues; col->cssValueId; ++col) {
        if (col->cssValueId == cssValueId)
            return col->color;
    }
    return RenderTheme::defaultTheme()->systemColor(cssValueId);
}

bool StyleResolver::colorFromPrimitiveValueIsDerivedFromElement(CSSPrimitiveValue* value)
{
    int ident = value->getValueID();
    switch (ident) {
    case CSSValueWebkitText:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

Color StyleResolver::colorFromPrimitiveValue(CSSPrimitiveValue* value, bool forVisitedLink) const
{
    if (value->isRGBColor())
        return Color(value->getRGBA32Value());

    const State& state = m_state;
    CSSValueID ident = value->getValueID();
    switch (ident) {
    case 0:
        return Color();
    case CSSValueWebkitText:
        return state.document().textColor();
    case CSSValueWebkitLink:
        return (state.element()->isLink() && forVisitedLink) ? state.document().visitedLinkColor() : state.document().linkColor();
    case CSSValueWebkitActivelink:
        return state.document().activeLinkColor();
    case CSSValueWebkitFocusRingColor:
        return RenderTheme::focusRingColor();
    case CSSValueCurrentcolor:
        return state.style()->color();
    default:
        return colorForCSSValue(ident);
    }
}

void StyleResolver::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(std::make_unique<MediaQueryResult>(*expr, result));
}

bool StyleResolver::affectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

#if ENABLE(CSS_FILTERS)
static FilterOperation::OperationType filterOperationForType(WebKitCSSFilterValue::FilterOperationType type)
{
    switch (type) {
    case WebKitCSSFilterValue::ReferenceFilterOperation:
        return FilterOperation::REFERENCE;
    case WebKitCSSFilterValue::GrayscaleFilterOperation:
        return FilterOperation::GRAYSCALE;
    case WebKitCSSFilterValue::SepiaFilterOperation:
        return FilterOperation::SEPIA;
    case WebKitCSSFilterValue::SaturateFilterOperation:
        return FilterOperation::SATURATE;
    case WebKitCSSFilterValue::HueRotateFilterOperation:
        return FilterOperation::HUE_ROTATE;
    case WebKitCSSFilterValue::InvertFilterOperation:
        return FilterOperation::INVERT;
    case WebKitCSSFilterValue::OpacityFilterOperation:
        return FilterOperation::OPACITY;
    case WebKitCSSFilterValue::BrightnessFilterOperation:
        return FilterOperation::BRIGHTNESS;
    case WebKitCSSFilterValue::ContrastFilterOperation:
        return FilterOperation::CONTRAST;
    case WebKitCSSFilterValue::BlurFilterOperation:
        return FilterOperation::BLUR;
    case WebKitCSSFilterValue::DropShadowFilterOperation:
        return FilterOperation::DROP_SHADOW;
    case WebKitCSSFilterValue::UnknownFilterOperation:
        return FilterOperation::NONE;
    }
    return FilterOperation::NONE;
}

void StyleResolver::loadPendingSVGDocuments()
{
    State& state = m_state;

    // Crash reports indicate that we've seen calls to this function when our
    // style is NULL. We don't know exactly why this happens. Our guess is
    // reentering styleForElement().
    ASSERT(state.style());
    if (!state.style() || !state.style()->hasFilter() || state.filtersWithPendingSVGDocuments().isEmpty())
        return;

    CachedResourceLoader* cachedResourceLoader = state.document().cachedResourceLoader();
    for (auto& filterOperation : state.filtersWithPendingSVGDocuments())
        filterOperation->getOrCreateCachedSVGDocumentReference()->load(cachedResourceLoader);

    state.filtersWithPendingSVGDocuments().clear();
}

bool StyleResolver::createFilterOperations(CSSValue* inValue, FilterOperations& outOperations)
{
    State& state = m_state;
    RenderStyle* style = state.style();
    RenderStyle* rootStyle = state.rootElementStyle();
    ASSERT(outOperations.isEmpty());
    
    if (!inValue)
        return false;
    
    if (inValue->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(inValue);
        if (primitiveValue->getValueID() == CSSValueNone)
            return true;
    }
    
    if (!inValue->isValueList())
        return false;

    float zoomFactor = style ? style->effectiveZoom() : 1;
    FilterOperations operations;
    for (CSSValueListIterator i = inValue; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (!currValue->isWebKitCSSFilterValue())
            continue;

        WebKitCSSFilterValue* filterValue = toWebKitCSSFilterValue(i.value());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue->operationType());

        if (operationType == FilterOperation::REFERENCE) {
            if (filterValue->length() != 1)
                continue;
            CSSValue* argument = filterValue->itemWithoutBoundsCheck(0);

            if (!argument->isPrimitiveValue())
                continue;

            CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(*argument);
            String cssUrl = primitiveValue.getStringValue();
            URL url = m_state.document().completeURL(cssUrl);

            RefPtr<ReferenceFilterOperation> operation = ReferenceFilterOperation::create(cssUrl, url.fragmentIdentifier(), operationType);
            if (SVGURIReference::isExternalURIReference(cssUrl, m_state.document()))
                state.filtersWithPendingSVGDocuments().append(operation);

            operations.operations().append(operation);
            continue;
        }

        // Check that all parameters are primitive values, with the
        // exception of drop shadow which has a CSSShadowValue parameter.
        CSSPrimitiveValue* firstValue = nullptr;
        if (operationType != FilterOperation::DROP_SHADOW) {
            bool haveNonPrimitiveValue = false;
            for (unsigned j = 0; j < filterValue->length(); ++j) {
                if (!filterValue->itemWithoutBoundsCheck(j)->isPrimitiveValue()) {
                    haveNonPrimitiveValue = true;
                    break;
                }
            }
            if (haveNonPrimitiveValue)
                continue;
            if (filterValue->length())
                firstValue = toCSSPrimitiveValue(filterValue->itemWithoutBoundsCheck(0));
        }

        switch (filterValue->operationType()) {
        case WebKitCSSFilterValue::GrayscaleFilterOperation:
        case WebKitCSSFilterValue::SepiaFilterOperation:
        case WebKitCSSFilterValue::SaturateFilterOperation: {
            double amount = 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::HueRotateFilterOperation: {
            double angle = 0;
            if (filterValue->length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case WebKitCSSFilterValue::InvertFilterOperation:
        case WebKitCSSFilterValue::BrightnessFilterOperation:
        case WebKitCSSFilterValue::ContrastFilterOperation:
        case WebKitCSSFilterValue::OpacityFilterOperation: {
            double amount = (filterValue->operationType() == WebKitCSSFilterValue::BrightnessFilterOperation) ? 0 : 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case WebKitCSSFilterValue::BlurFilterOperation: {
            Length stdDeviation = Length(0, Fixed);
            if (filterValue->length() >= 1)
                stdDeviation = convertToFloatLength(firstValue, style, rootStyle, zoomFactor);
            if (stdDeviation.isUndefined())
                return false;

            operations.operations().append(BlurFilterOperation::create(stdDeviation, operationType));
            break;
        }
        case WebKitCSSFilterValue::DropShadowFilterOperation: {
            if (filterValue->length() != 1)
                return false;

            CSSValue* cssValue = filterValue->itemWithoutBoundsCheck(0);
            if (!cssValue->isShadowValue())
                continue;

            CSSShadowValue* item = toCSSShadowValue(cssValue);
            int x = item->x->computeLength<int>(style, rootStyle, zoomFactor);
            if (item->x->isViewportPercentageLength())
                x = viewportPercentageValue(*item->x, x);
            int y = item->y->computeLength<int>(style, rootStyle, zoomFactor);
            if (item->y->isViewportPercentageLength())
                y = viewportPercentageValue(*item->y, y);
            IntPoint location(x, y);
            int blur = item->blur ? item->blur->computeLength<int>(style, rootStyle, zoomFactor) : 0;
            if (item->blur && item->blur->isViewportPercentageLength())
                blur = viewportPercentageValue(*item->blur, blur);
            Color color;
            if (item->color)
                color = colorFromPrimitiveValue(item->color.get());

            operations.operations().append(DropShadowFilterOperation::create(location, blur, color.isValid() ? color : Color::transparent, operationType));
            break;
        }
        case WebKitCSSFilterValue::UnknownFilterOperation:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

#endif

PassRefPtr<StyleImage> StyleResolver::loadPendingImage(const StylePendingImage& pendingImage, const ResourceLoaderOptions& options)
{
    if (auto imageValue = pendingImage.cssImageValue())
        return imageValue->cachedImage(m_state.document().cachedResourceLoader(), options);

    if (auto imageGeneratorValue = pendingImage.cssImageGeneratorValue()) {
        imageGeneratorValue->loadSubimages(m_state.document().cachedResourceLoader());
        return StyleGeneratedImage::create(*imageGeneratorValue);
    }

    if (auto cursorImageValue = pendingImage.cssCursorImageValue())
        return cursorImageValue->cachedImage(m_state.document().cachedResourceLoader());

#if ENABLE(CSS_IMAGE_SET)
    if (auto imageSetValue = pendingImage.cssImageSetValue())
        return imageSetValue->cachedImageSet(m_state.document().cachedResourceLoader(), options);
#endif

    return nullptr;
}

PassRefPtr<StyleImage> StyleResolver::loadPendingImage(const StylePendingImage& pendingImage)
{
    return loadPendingImage(pendingImage, CachedResourceLoader::defaultCachedResourceOptions());
}

#if ENABLE(CSS_SHAPES)
void StyleResolver::loadPendingShapeImage(ShapeValue* shapeValue)
{
    if (!shapeValue)
        return;

    StyleImage* image = shapeValue->image();
    if (!image || !image->isPendingImage())
        return;

    auto& pendingImage = toStylePendingImage(*image);

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.requestOriginPolicy = PotentiallyCrossOriginEnabled;
    options.allowCredentials = DoNotAllowStoredCredentials;

    shapeValue->setImage(loadPendingImage(pendingImage, options));
}
#endif

void StyleResolver::loadPendingImages()
{
    if (m_state.pendingImageProperties().isEmpty())
        return;

    auto end = m_state.pendingImageProperties().end().keys();
    for (auto it = m_state.pendingImageProperties().begin().keys(); it != end; ++it) {
        CSSPropertyID currentProperty = *it;

        switch (currentProperty) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = m_state.style()->accessBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                auto styleImage = backgroundLayer->image();
                if (styleImage && styleImage->isPendingImage())
                    backgroundLayer->setImage(loadPendingImage(toStylePendingImage(*styleImage)));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(m_state.style()->contentData()); contentData; contentData = contentData->next()) {
                if (contentData->isImage()) {
                    auto& styleImage = toImageContentData(contentData)->image();
                    if (styleImage.isPendingImage()) {
                        RefPtr<StyleImage> loadedImage = loadPendingImage(toStylePendingImage(styleImage));
                        if (loadedImage)
                            toImageContentData(contentData)->setImage(loadedImage.release());
                    }
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = m_state.style()->cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    auto styleImage = currentCursor.image();
                    if (styleImage && styleImage->isPendingImage())
                        currentCursor.setImage(loadPendingImage(toStylePendingImage(*styleImage)));
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            auto styleImage = m_state.style()->listStyleImage();
            if (styleImage && styleImage->isPendingImage())
                m_state.style()->setListStyleImage(loadPendingImage(toStylePendingImage(*styleImage)));
            break;
        }
        case CSSPropertyBorderImageSource: {
            auto styleImage = m_state.style()->borderImageSource();
            if (styleImage && styleImage->isPendingImage())
                m_state.style()->setBorderImageSource(loadPendingImage(toStylePendingImage(*styleImage)));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = m_state.style()->boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                auto styleImage = maskImage.image();
                if (styleImage && styleImage->isPendingImage()) {
                    RefPtr<StyleImage> loadedImage = loadPendingImage(toStylePendingImage(*styleImage));
                    reflection->setMask(NinePieceImage(loadedImage.release(), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            auto styleImage = m_state.style()->maskBoxImageSource();
            if (styleImage && styleImage->isPendingImage())
                m_state.style()->setMaskBoxImageSource(loadPendingImage(toStylePendingImage(*styleImage)));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = m_state.style()->accessMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                auto styleImage = maskLayer->image();
                if (styleImage && styleImage->isPendingImage())
                    maskLayer->setImage(loadPendingImage(toStylePendingImage(*styleImage)));
            }
            break;
        }
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
        case CSSPropertyWebkitShapeInside:
            loadPendingShapeImage(m_state.style()->shapeInside());
            break;
#endif
#if ENABLE(CSS_SHAPES)
        case CSSPropertyWebkitShapeOutside:
            loadPendingShapeImage(m_state.style()->shapeOutside());
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
        }
    }

    m_state.pendingImageProperties().clear();
}

#ifndef NDEBUG
static bool inLoadPendingResources = false;
#endif

void StyleResolver::loadPendingResources()
{
    // We've seen crashes in all three of the functions below. Some of them
    // indicate that style() is NULL. This NULL check will cut down on total
    // crashes, while the ASSERT will help us find the cause in debug builds.
    ASSERT(style());
    if (!style())
        return;

#ifndef NDEBUG
    // Re-entering this function will probably mean trouble. Catch it in debug builds.
    ASSERT(!inLoadPendingResources);
    inLoadPendingResources = true;
#endif

    // Start loading images referenced by this style.
    loadPendingImages();

#if ENABLE(CSS_FILTERS)
    // Start loading the SVG Documents referenced by this style.
    loadPendingSVGDocuments();
#endif

#ifndef NDEBUG
    inLoadPendingResources = false;
#endif
}

inline StyleResolver::MatchedProperties::MatchedProperties()
    : possiblyPaddedMember(0)
{
}

StyleResolver::MatchedProperties::~MatchedProperties()
{
}

int StyleResolver::viewportPercentageValue(CSSPrimitiveValue& unit, int percentage)
{
    int viewPortHeight = document().renderView()->viewportSize().height() * percentage / 100.0f;
    int viewPortWidth = document().renderView()->viewportSize().width() * percentage / 100.0f;

    if (unit.isViewportPercentageHeight())
        return viewPortHeight;
    if (unit.isViewportPercentageWidth())
        return viewPortWidth;
    if (unit.isViewportPercentageMax())
        return std::max(viewPortWidth, viewPortHeight);
    if (unit.isViewportPercentageMin())
        return std::min(viewPortWidth, viewPortHeight);

    ASSERT_NOT_REACHED();
    return 0;
}

StyleResolver::CascadedProperties::CascadedProperties(TextDirection direction, WritingMode writingMode)
    : m_direction(direction)
    , m_writingMode(writingMode)
{
}

inline StyleResolver::CascadedProperties::Property& StyleResolver::CascadedProperties::property(CSSPropertyID id)
{
    return m_properties[id];
}

void StyleResolver::CascadedProperties::setPropertyInternal(Property& property, CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType)
{
    ASSERT(linkMatchType <= SelectorChecker::MatchAll);
    property.id = id;
    if (linkMatchType == SelectorChecker::MatchAll) {
        property.cssValue[0] = &cssValue;
        property.cssValue[SelectorChecker::MatchLink] = &cssValue;
        property.cssValue[SelectorChecker::MatchVisited] = &cssValue;
    } else
        property.cssValue[linkMatchType] = &cssValue;
}

void StyleResolver::CascadedProperties::set(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType)
{
    if (CSSProperty::isDirectionAwareProperty(id))
        id = CSSProperty::resolveDirectionAwareProperty(id, m_direction, m_writingMode);

    ASSERT(!shouldApplyPropertyInParseOrder(id));

    auto& property = m_properties[id];
    if (!m_propertyIsPresent.test(id))
        memset(property.cssValue, 0, sizeof(property.cssValue));
    m_propertyIsPresent.set(id);
    setPropertyInternal(property, id, cssValue, linkMatchType);
}

void StyleResolver::CascadedProperties::setDeferred(CSSPropertyID id, CSSValue& cssValue, unsigned linkMatchType)
{
    ASSERT(!CSSProperty::isDirectionAwareProperty(id));
    ASSERT(shouldApplyPropertyInParseOrder(id));

    Property property;
    memset(property.cssValue, 0, sizeof(property.cssValue));
    setPropertyInternal(property, id, cssValue, linkMatchType);
    m_deferredProperties.append(property);
}

bool StyleResolver::CascadedProperties::addStyleProperties(const StyleProperties& properties, StyleRule&, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType, unsigned linkMatchType)
{
    for (unsigned i = 0, count = properties.propertyCount(); i < count; ++i) {
        auto current = properties.propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            if (current.value()->isInheritedValue())
                return false;
            continue;
        }
        CSSPropertyID propertyID = current.id();

        if (propertyWhitelistType == PropertyWhitelistRegion && !StyleResolver::isValidRegionStyleProperty(propertyID))
            continue;
#if ENABLE(VIDEO_TRACK)
        if (propertyWhitelistType == PropertyWhitelistCue && !StyleResolver::isValidCueStyleProperty(propertyID))
            continue;
#endif

        if (shouldApplyPropertyInParseOrder(propertyID))
            setDeferred(propertyID, *current.value(), linkMatchType);
        else
            set(propertyID, *current.value(), linkMatchType);
    }
    return true;
}

bool StyleResolver::CascadedProperties::addMatches(const MatchResult& matchResult, bool important, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return true;

    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        if (!addStyleProperties(*matchedProperties.properties, *matchResult.matchedRules[i], important, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType), matchedProperties.linkMatchType))
            return false;
    }
    return true;
}

void StyleResolver::CascadedProperties::applyDeferredProperties(StyleResolver& resolver)
{
    for (auto& property : m_deferredProperties)
        property.apply(resolver);
}

void StyleResolver::CascadedProperties::Property::apply(StyleResolver& resolver)
{
    State& state = resolver.state();

    // FIXME: It would be nice if line-height were less of a special snowflake.
    if (id == CSSPropertyLineHeight) {
        if (auto value = state.style()->insideLink() == NotInsideLink ? cssValue[0] : cssValue[SelectorChecker::MatchLink])
            state.setLineHeightValue(value);
        return;
    }

    if (cssValue[0]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[0]);
    }

    if (state.style()->insideLink() == NotInsideLink)
        return;

    if (cssValue[SelectorChecker::MatchLink]) {
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchLink]);
    }

    if (cssValue[SelectorChecker::MatchVisited]) {
        state.setApplyPropertyToRegularStyle(false);
        state.setApplyPropertyToVisitedLinkStyle(true);
        resolver.applyProperty(id, cssValue[SelectorChecker::MatchVisited]);
    }

    state.setApplyPropertyToRegularStyle(true);
    state.setApplyPropertyToVisitedLinkStyle(false);
}

void StyleResolver::applyCascadedProperties(CascadedProperties& cascade, int firstProperty, int lastProperty)
{
    for (int id = firstProperty; id <= lastProperty; ++id) {
        CSSPropertyID propertyID = static_cast<CSSPropertyID>(id);
        if (!cascade.hasProperty(propertyID))
            continue;
        auto& property = cascade.property(propertyID);
        ASSERT(!shouldApplyPropertyInParseOrder(propertyID));
        property.apply(*this);
    }
}

} // namespace WebCore

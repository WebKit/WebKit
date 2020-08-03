/*
 * Copyright (C) 2006, 2008, 2011-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "HitTestResult.h"

#include "CachedImage.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "File.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "PseudoElement.h"
#include "Range.h"
#include "RenderBlockFlow.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "SVGAElement.h"
#include "SVGImageElement.h"
#include "Scrollbar.h"
#include "ShadowRoot.h"
#include "TextIterator.h"
#include "UserGestureIndicator.h"
#include "VisibleUnits.h"
#include "XLinkNames.h"

namespace WebCore {

using namespace HTMLNames;

static inline void appendToNodeSet(const HitTestResult::NodeSet& source, HitTestResult::NodeSet& destination)
{
    for (auto& node : source)
        destination.add(node.copyRef());
}

HitTestResult::HitTestResult() = default;

HitTestResult::HitTestResult(const LayoutPoint& point)
    : m_hitTestLocation(point)
    , m_pointInInnerNodeFrame(point)
{
}

HitTestResult::HitTestResult(const LayoutRect& rect)
    : m_hitTestLocation { rect }
    , m_pointInInnerNodeFrame { rect.center() }
{
}

HitTestResult::HitTestResult(const LayoutPoint& centerPoint, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding)
    : m_hitTestLocation(centerPoint, topPadding, rightPadding, bottomPadding, leftPadding)
    , m_pointInInnerNodeFrame(centerPoint)
{
}

HitTestResult::HitTestResult(const HitTestLocation& other)
    : m_hitTestLocation(other)
    , m_pointInInnerNodeFrame(m_hitTestLocation.point())
{
}

HitTestResult::HitTestResult(const HitTestResult& other)
    : m_hitTestLocation(other.m_hitTestLocation)
    , m_innerNode(other.innerNode())
    , m_innerNonSharedNode(other.innerNonSharedNode())
    , m_pointInInnerNodeFrame(other.m_pointInInnerNodeFrame)
    , m_localPoint(other.localPoint())
    , m_innerURLElement(other.URLElement())
    , m_scrollbar(other.scrollbar())
    , m_isOverWidget(other.isOverWidget())
{
    // Only copy the NodeSet in case of list hit test.
    if (other.m_listBasedTestResult) {
        m_listBasedTestResult = makeUnique<NodeSet>();
        appendToNodeSet(*other.m_listBasedTestResult, *m_listBasedTestResult);
    }
}

HitTestResult::~HitTestResult() = default;

HitTestResult& HitTestResult::operator=(const HitTestResult& other)
{
    m_hitTestLocation = other.m_hitTestLocation;
    m_innerNode = other.innerNode();
    m_innerNonSharedNode = other.innerNonSharedNode();
    m_pointInInnerNodeFrame = other.m_pointInInnerNodeFrame;
    m_localPoint = other.localPoint();
    m_innerURLElement = other.URLElement();
    m_scrollbar = other.scrollbar();
    m_isOverWidget = other.isOverWidget();

    // Only copy the NodeSet in case of list hit test.
    if (other.m_listBasedTestResult) {
        m_listBasedTestResult = makeUnique<NodeSet>();
        appendToNodeSet(*other.m_listBasedTestResult, *m_listBasedTestResult);
    }

    return *this;
}

static Node* moveOutOfUserAgentShadowTree(Node& node)
{
    if (node.isInShadowTree()) {
        if (ShadowRoot* root = node.containingShadowRoot()) {
            if (root->mode() == ShadowRootMode::UserAgent)
                return root->host();
        }
    }
    return &node;
}

void HitTestResult::setToNonUserAgentShadowAncestor()
{
    if (Node* node = innerNode()) {
        node = moveOutOfUserAgentShadowTree(*node);
        setInnerNode(node);
    }
    if (Node *node = innerNonSharedNode()) {
        node = moveOutOfUserAgentShadowTree(*node);
        setInnerNonSharedNode(node);
    }
}

void HitTestResult::setInnerNode(Node* node)
{
    if (is<PseudoElement>(node))
        node = downcast<PseudoElement>(*node).hostElement();
    m_innerNode = node;
}
    
void HitTestResult::setInnerNonSharedNode(Node* node)
{
    if (is<PseudoElement>(node))
        node = downcast<PseudoElement>(*node).hostElement();
    m_innerNonSharedNode = node;
}

void HitTestResult::setURLElement(Element* n) 
{ 
    m_innerURLElement = n; 
}

void HitTestResult::setScrollbar(Scrollbar* s)
{
    m_scrollbar = s;
}

Frame* HitTestResult::innerNodeFrame() const
{
    if (m_innerNonSharedNode)
        return m_innerNonSharedNode->document().frame();
    if (m_innerNode)
        return m_innerNode->document().frame();
    return 0;
}

Frame* HitTestResult::targetFrame() const
{
    if (!m_innerURLElement)
        return nullptr;

    Frame* frame = m_innerURLElement->document().frame();
    if (!frame)
        return nullptr;

    return frame->tree().find(m_innerURLElement->target(), *frame);
}

bool HitTestResult::isSelected() const
{
    if (!m_innerNonSharedNode)
        return false;

    Frame* frame = m_innerNonSharedNode->document().frame();
    if (!frame)
        return false;

    return frame->selection().contains(m_hitTestLocation.point());
}

String HitTestResult::selectedText() const
{
    if (!m_innerNonSharedNode)
        return emptyString();

    Frame* frame = m_innerNonSharedNode->document().frame();
    if (!frame)
        return emptyString();

    auto range = frame->selection().selection().toNormalizedRange();
    if (!range)
        return emptyString();

    // Look for a character that's not just a separator.
    for (TextIterator it(*range); !it.atEnd(); it.advance()) {
        int length = it.text().length();
        for (int i = 0; i < length; ++i) {
            if (!(U_GET_GC_MASK(it.text()[i]) & U_GC_Z_MASK))
                return frame->displayStringModifiedByEncoding(frame->editor().selectedText());
        }
    }
    return emptyString();
}

String HitTestResult::spellingToolTip(TextDirection& dir) const
{
    dir = TextDirection::LTR;
    // Return the tool tip string associated with this point, if any. Only markers associated with bad grammar
    // currently supply strings, but maybe someday markers associated with misspelled words will also.
    if (!m_innerNonSharedNode)
        return String();
    
    DocumentMarker* marker = m_innerNonSharedNode->document().markers().markerContainingPoint(m_hitTestLocation.point(), DocumentMarker::Grammar);
    if (!marker)
        return String();

    if (auto renderer = m_innerNonSharedNode->renderer())
        dir = renderer->style().direction();
    return marker->description();
}

String HitTestResult::replacedString() const
{
    // Return the replaced string associated with this point, if any. This marker is created when a string is autocorrected, 
    // and is used for generating a contextual menu item that allows it to easily be changed back if desired.
    if (!m_innerNonSharedNode)
        return String();
    
    DocumentMarker* marker = m_innerNonSharedNode->document().markers().markerContainingPoint(m_hitTestLocation.point(), DocumentMarker::Replacement);
    if (!marker)
        return String();
    
    return marker->description();
}    
    
String HitTestResult::title(TextDirection& dir) const
{
    dir = TextDirection::LTR;
    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (Node* titleNode = m_innerNode.get(); titleNode; titleNode = titleNode->parentInComposedTree()) {
        if (is<Element>(*titleNode)) {
            Element& titleElement = downcast<Element>(*titleNode);
            String title = titleElement.title();
            if (!title.isEmpty()) {
                if (auto renderer = titleElement.renderer())
                    dir = renderer->style().direction();
                return title;
            }
        }
    }
    return String();
}

String HitTestResult::innerTextIfTruncated(TextDirection& dir) const
{
    for (Node* truncatedNode = m_innerNode.get(); truncatedNode; truncatedNode = truncatedNode->parentInComposedTree()) {
        if (!is<Element>(*truncatedNode))
            continue;

        if (auto renderer = downcast<Element>(*truncatedNode).renderer()) {
            if (is<RenderBlockFlow>(*renderer)) {
                RenderBlockFlow& block = downcast<RenderBlockFlow>(*renderer);
                if (block.style().textOverflow() == TextOverflow::Ellipsis) {
                    for (RootInlineBox* line = block.firstRootBox(); line; line = line->nextRootBox()) {
                        if (line->hasEllipsisBox()) {
                            dir = block.style().direction();
                            return downcast<Element>(*truncatedNode).innerText();
                        }
                    }
                }
                break;
            }
        }
    }

    dir = TextDirection::LTR;
    return String();
}

String displayString(const String& string, const Node* node)
{
    if (!node)
        return string;
    return node->document().displayStringModifiedByEncoding(string);
}

String HitTestResult::altDisplayString() const
{
    if (!m_innerNonSharedNode)
        return String();
    
    if (is<HTMLImageElement>(*m_innerNonSharedNode)) {
        HTMLImageElement& image = downcast<HTMLImageElement>(*m_innerNonSharedNode);
        return displayString(image.attributeWithoutSynchronization(altAttr), m_innerNonSharedNode.get());
    }
    
    if (is<HTMLInputElement>(*m_innerNonSharedNode)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*m_innerNonSharedNode);
        return displayString(input.alt(), m_innerNonSharedNode.get());
    }

    return String();
}

Image* HitTestResult::image() const
{
    if (!m_innerNonSharedNode)
        return nullptr;
    
    auto* renderer = m_innerNonSharedNode->renderer();
    if (is<RenderImage>(renderer)) {
        auto& image = downcast<RenderImage>(*renderer);
        if (image.cachedImage() && !image.cachedImage()->errorOccurred())
            return image.cachedImage()->imageForRenderer(&image);
    }

    return nullptr;
}

IntRect HitTestResult::imageRect() const
{
    if (!image())
        return IntRect();
    return m_innerNonSharedNode->renderBox()->absoluteContentQuad().enclosingBoundingBox();
}

URL HitTestResult::absoluteImageURL() const
{
    if (!m_innerNonSharedNode)
        return URL();

    if (!(m_innerNonSharedNode->renderer() && m_innerNonSharedNode->renderer()->isImage()))
        return URL();

    AtomString urlString;
    if (is<HTMLEmbedElement>(*m_innerNonSharedNode)
        || is<HTMLImageElement>(*m_innerNonSharedNode)
        || is<HTMLInputElement>(*m_innerNonSharedNode)
        || is<HTMLObjectElement>(*m_innerNonSharedNode)
        || is<SVGImageElement>(*m_innerNonSharedNode)) {
        urlString = downcast<Element>(*m_innerNonSharedNode).imageSourceURL();
    } else
        return URL();

    return m_innerNonSharedNode->document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
}

URL HitTestResult::absolutePDFURL() const
{
    if (!m_innerNonSharedNode)
        return URL();

    if (!is<HTMLEmbedElement>(*m_innerNonSharedNode) && !is<HTMLObjectElement>(*m_innerNonSharedNode))
        return URL();

    HTMLPlugInImageElement& element = downcast<HTMLPlugInImageElement>(*m_innerNonSharedNode);
    URL url = m_innerNonSharedNode->document().completeURL(stripLeadingAndTrailingHTMLSpaces(element.url()));
    if (!url.isValid())
        return URL();

    if (element.serviceType() == "application/pdf" || (element.serviceType().isEmpty() && url.path().endsWithIgnoringASCIICase(".pdf")))
        return url;
    return URL();
}

URL HitTestResult::absoluteMediaURL() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->currentSrc();
    return URL();
#else
    return URL();
#endif
}

bool HitTestResult::mediaSupportsFullscreen() const
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElt(mediaElement());
    return is<HTMLVideoElement>(mediaElt) && mediaElt->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);
#else
    return false;
#endif
}

#if ENABLE(VIDEO)
HTMLMediaElement* HitTestResult::mediaElement() const
{
    if (!m_innerNonSharedNode)
        return nullptr;

    if (!(m_innerNonSharedNode->renderer() && m_innerNonSharedNode->renderer()->isMedia()))
        return nullptr;

    if (is<HTMLMediaElement>(*m_innerNonSharedNode))
        return downcast<HTMLMediaElement>(m_innerNonSharedNode.get());
    return nullptr;
}
#endif

void HitTestResult::toggleMediaControlsDisplay() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        mediaElt->setControls(!mediaElt->controls());
#endif
}

void HitTestResult::toggleMediaLoopPlayback() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        mediaElt->setLoop(!mediaElt->loop());
#endif
}

bool HitTestResult::mediaIsInFullscreen() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElement = this->mediaElement())
        return mediaElement->isVideo() && mediaElement->isStandardFullscreen();
#endif
    return false;
}

void HitTestResult::toggleMediaFullscreenState() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElement = this->mediaElement()) {
        if (mediaElement->isVideo() && mediaElement->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
            UserGestureIndicator indicator(ProcessingUserGesture, &mediaElement->document());
            mediaElement->toggleStandardFullscreenState();
        }
    }
#endif
}

void HitTestResult::enterFullscreenForVideo() const
{
#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement(this->mediaElement());
    if (is<HTMLVideoElement>(mediaElement)) {
        HTMLVideoElement& videoElement = downcast<HTMLVideoElement>(*mediaElement);
        if (!videoElement.isFullscreen() && mediaElement->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
            UserGestureIndicator indicator(ProcessingUserGesture, &mediaElement->document());
            videoElement.enterFullscreen();
        }
    }
#endif
}

bool HitTestResult::mediaControlsEnabled() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElement = this->mediaElement())
        return mediaElement->controls();
#endif
    return false;
}

bool HitTestResult::mediaLoopEnabled() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->loop();
#endif
    return false;
}

bool HitTestResult::mediaPlaying() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return !mediaElt->paused();
#endif
    return false;
}

void HitTestResult::toggleMediaPlayState() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        mediaElt->togglePlayState();
#endif
}

bool HitTestResult::mediaHasAudio() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->hasAudio();
#endif
    return false;
}

bool HitTestResult::mediaIsVideo() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return is<HTMLVideoElement>(*mediaElt);
#endif
    return false;
}

bool HitTestResult::mediaMuted() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->muted();
#endif
    return false;
}

void HitTestResult::toggleMediaMuteState() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        mediaElt->setMuted(!mediaElt->muted());
#endif
}

bool HitTestResult::isDownloadableMedia() const
{
#if ENABLE(VIDEO)
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->canSaveMediaData();
#endif

    return false;
}

bool HitTestResult::isOverTextInsideFormControlElement() const
{
    Node* node = innerNode();
    if (!node)
        return false;

    if (!is<Element>(*node) || !downcast<Element>(*node).isTextField())
        return false;

    Frame* frame = node->document().frame();
    if (!frame)
        return false;

    IntPoint framePoint = roundedPointInInnerNodeFrame();
    if (!frame->rangeForPoint(framePoint))
        return false;

    VisiblePosition position = frame->visiblePositionForPoint(framePoint);
    if (position.isNull())
        return false;

    auto wordRange = enclosingTextUnitOfGranularity(position, TextGranularity::WordGranularity, SelectionDirection::Forward);
    return wordRange && hasAnyPlainText(*wordRange);
}

URL HitTestResult::absoluteLinkURL() const
{
    if (m_innerURLElement)
        return m_innerURLElement->absoluteLinkURL();
    return URL();
}

bool HitTestResult::isOverLink() const
{
    return m_innerURLElement && m_innerURLElement->isLink();
}

String HitTestResult::titleDisplayString() const
{
    if (!m_innerURLElement)
        return String();
    
    return displayString(m_innerURLElement->title(), m_innerURLElement.get());
}

String HitTestResult::textContent() const
{
    if (!m_innerURLElement)
        return String();
    return m_innerURLElement->textContent();
}

// FIXME: This function needs a better name and may belong in a different class. It's not
// really isContentEditable(); it's more like needsEditingContextMenu(). In many ways, this
// function would make more sense in the ContextMenu class, except that WebElementDictionary 
// hooks into it. Anyway, we should architect this better. 
bool HitTestResult::isContentEditable() const
{
    if (!m_innerNonSharedNode)
        return false;

    if (is<HTMLTextAreaElement>(*m_innerNonSharedNode))
        return true;

    if (is<HTMLInputElement>(*m_innerNonSharedNode))
        return downcast<HTMLInputElement>(*m_innerNonSharedNode).isTextField();

    return m_innerNonSharedNode->hasEditableStyle();
}

template<typename RectType>
inline HitTestProgress HitTestResult::addNodeToListBasedTestResultCommon(Node* node, const HitTestRequest& request, const HitTestLocation& locationInContainer, const RectType& rect)
{
    // If it is not a list-based hit test, this method has to be no-op.
    if (!request.resultIsElementList()) {
        ASSERT(!isRectBasedTest());
        return HitTestProgress::Stop;
    }

    if (!node)
        return HitTestProgress::Continue;

    if (request.disallowsUserAgentShadowContent() && node->isInUserAgentShadowTree())
        node = node->document().ancestorNodeInThisScope(node);

    mutableListBasedTestResult().add(*node);

    if (request.includesAllElementsUnderPoint())
        return HitTestProgress::Continue;

    bool regionFilled = rect.contains(locationInContainer.boundingBox());
    return regionFilled ? HitTestProgress::Stop : HitTestProgress::Continue;
}

HitTestProgress HitTestResult::addNodeToListBasedTestResult(Node* node, const HitTestRequest& request, const HitTestLocation& locationInContainer, const LayoutRect& rect)
{
    return addNodeToListBasedTestResultCommon(node, request, locationInContainer, rect);
}

HitTestProgress HitTestResult::addNodeToListBasedTestResult(Node* node, const HitTestRequest& request, const HitTestLocation& locationInContainer, const FloatRect& rect)
{
    return addNodeToListBasedTestResultCommon(node, request, locationInContainer, rect);
}

void HitTestResult::append(const HitTestResult& other, const HitTestRequest& request)
{
    ASSERT_UNUSED(request, request.resultIsElementList());

    if (!m_innerNode && other.innerNode()) {
        m_innerNode = other.innerNode();
        m_innerNonSharedNode = other.innerNonSharedNode();
        m_localPoint = other.localPoint();
        m_pointInInnerNodeFrame = other.m_pointInInnerNodeFrame;
        m_innerURLElement = other.URLElement();
        m_scrollbar = other.scrollbar();
        m_isOverWidget = other.isOverWidget();
    }

    if (other.m_listBasedTestResult)
        appendToNodeSet(*other.m_listBasedTestResult, mutableListBasedTestResult());
}

const HitTestResult::NodeSet& HitTestResult::listBasedTestResult() const
{
    if (!m_listBasedTestResult)
        m_listBasedTestResult = makeUnique<NodeSet>();
    return *m_listBasedTestResult;
}

HitTestResult::NodeSet& HitTestResult::mutableListBasedTestResult()
{
    if (!m_listBasedTestResult)
        m_listBasedTestResult = makeUnique<NodeSet>();
    return *m_listBasedTestResult;
}

Vector<String> HitTestResult::dictationAlternatives() const
{
    // Return the dictation context handle if the text at this point has DictationAlternative marker, which means this text is
    if (!m_innerNonSharedNode)
        return Vector<String>();

    DocumentMarker* marker = m_innerNonSharedNode->document().markers().markerContainingPoint(pointInInnerNodeFrame(), DocumentMarker::DictationAlternatives);
    if (!marker)
        return Vector<String>();

    Frame* frame = innerNonSharedNode()->document().frame();
    if (!frame)
        return Vector<String>();

    return frame->editor().dictationAlternativesForMarker(*marker);
}

Node* HitTestResult::targetNode() const
{
    Node* node = innerNode();
    if (!node)
        return nullptr;
    if (node->isConnected())
        return node;

    Element* element = node->parentElement();
    if (element && element->isConnected())
        return element;

    return node;
}

Element* HitTestResult::targetElement() const
{
    for (Node* node = m_innerNode.get(); node; node = node->parentInComposedTree()) {
        if (is<Element>(*node))
            return downcast<Element>(node);
    }
    return nullptr;
}

Element* HitTestResult::innerNonSharedElement() const
{
    Node* node = m_innerNonSharedNode.get();
    if (!node)
        return nullptr;
    if (is<Element>(*node))
        return downcast<Element>(node);
    return node->parentElement();
}

String HitTestResult::linkSuggestedFilename() const
{
    auto* urlElement = URLElement();
    if (!is<HTMLAnchorElement>(urlElement))
        return nullAtom();
    return ResourceResponse::sanitizeSuggestedFilename(urlElement->attributeWithoutSynchronization(HTMLNames::downloadAttr));
}

bool HitTestResult::mediaSupportsEnhancedFullscreen() const
{
#if PLATFORM(MAC) && ENABLE(VIDEO) && ENABLE(VIDEO_PRESENTATION_MODE)
    HTMLMediaElement* mediaElt(mediaElement());
    return is<HTMLVideoElement>(mediaElt) && mediaElt->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
#else
    return false;
#endif
}

bool HitTestResult::mediaIsInEnhancedFullscreen() const
{
#if PLATFORM(MAC) && ENABLE(VIDEO) && ENABLE(VIDEO_PRESENTATION_MODE)
    HTMLMediaElement* mediaElt(mediaElement());
    return is<HTMLVideoElement>(mediaElt) && mediaElt->fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture;
#else
    return false;
#endif
}

void HitTestResult::toggleEnhancedFullscreenForVideo() const
{
#if PLATFORM(MAC) && ENABLE(VIDEO) && ENABLE(VIDEO_PRESENTATION_MODE)
    HTMLMediaElement* mediaElement(this->mediaElement());
    if (!is<HTMLVideoElement>(mediaElement) || !mediaElement->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture))
        return;

    HTMLVideoElement& videoElement = downcast<HTMLVideoElement>(*mediaElement);
    UserGestureIndicator indicator(ProcessingUserGesture, &mediaElement->document());
    if (videoElement.fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        videoElement.exitFullscreen();
    else
        videoElement.enterFullscreen(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
#endif
}

} // namespace WebCore

/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AXFormActivityMonitor.h"

#if PLATFORM(COCOA)

#include "AXAttributeCacheScope.h"
#include "AXUtilities.h"
#include "AXObjectCacheInlines.h"
#include "AccessibilityObject.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "ComposedTreeIterator.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "HTMLBodyElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "Logging.h"
#include "RenderText.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "ValidatedFormListedElement.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXFormActivityMonitor);

bool shouldLogFormActivity()
{
#if !LOG_DISABLED
    if (LOG_CHANNEL(AccessibilityFormErrors).state != logChannelStateOff)
        return true;
#endif
    return AXObjectCache::isAppleInternalInstall();
}

void logFormActivity(const String& message)
{
#if !LOG_DISABLED
    if (LOG_CHANNEL(AccessibilityFormErrors).state != logChannelStateOff) {
        LOG(AccessibilityFormErrors, "%s", message.utf8());
        return;
    }
#endif
    RELEASE_LOG(AccessibilityFormErrors, "%" PUBLIC_LOG_STRING, message.utf8());
}

// How long to keep watching after a submission attempt. Long enough for a server round-trip to come
// back and render a message, short enough that later, unrelated page changes are not attributed to
// the submission.
static constexpr Seconds defaultSettleDelay { 3_s };
static std::optional<Seconds> settleDelayForTesting;

// A page can mutate text continuously. Cap what is collected so a runaway page cannot grow this
// without bound, and so the announcement stays short enough to be useful.
static constexpr size_t maximumCandidateErrorMessages = 32;
static constexpr unsigned maximumErrorMessageLength = 150;

// How many changed elements are worth remembering across the window, and how many accessibility objects
// are worth looking at once it closes.
static constexpr unsigned maximumChangedElements = 4096;
static constexpr unsigned maximumAnnouncedText = 128;

// The most the window may be stretched by content still arriving, as a multiple of the settle delay.
static constexpr unsigned maximumWatchMultiplier = 3;
static constexpr unsigned maximumObjectsVisited = 6000;
// Deep enough for a message wrapped in a few presentational spans, shallow enough that a whole page
// section cannot pass by being made entirely of text.
static constexpr unsigned maximumErrorMessageDepth = 4;

// Roles that structure a document. An error message is never one of these, and a container that holds
// one is an aggregate rather than a message.
static bool isStructural(AXCoreObject& object)
{
    switch (object.role()) {
    case AccessibilityRole::Heading:
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::List:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::Table:
    case AccessibilityRole::TabPanel:
        return true;
    default:
        return false;
    }
}

// Whether this is text that names a control rather than saying something about it.
static bool namesAControl(AXCoreObject& object)
{
    return object.role() == AccessibilityRole::Label || !object.labelForObjects().isEmpty();
}

// Returns false as soon as content appears that an error message would not contain.
static bool containsNonMessageContent(AXCoreObject& object, bool& foundStaticText, unsigned depth)
{
    if (depth > maximumErrorMessageDepth)
        return false;

    // Deliberately stored in a local, since namesAControl() resolves relations, which can re-build
    // children mid-walk.
    auto children = object.unignoredChildren();
    for (const Ref<AXCoreObject>& child : children) {
        if (namesAControl(child.get())) {
            // Labels for a control are not error messages.
            return true;
        }

        if (child->isStaticText()) {
            foundStaticText = true;
            continue;
        }

        // Some sites put a warning / error icon next to the message, so keep searching.
        if (child->isImage())
            continue;

        // Anything the user can act on, or that structures a document, means this is an aggregate that
        // merely happens to contain the message rather than being the message.
        if (child->isControl() || child->isLink() || isStructural(child.get()))
            return true;

        // Everything else is a presentational wrapper. Sites nest messages in spans and divs, which are
        // Generic rather than Group, so this cannot be an allow-list of container roles.
        if (containsNonMessageContent(child.get(), foundStaticText, depth + 1))
            return true;
    }
    return false;
}

static bool isErrorMessageShaped(AXCoreObject& object)
{
    if (namesAControl(object))
        return false;

    if (object.isStaticText())
        return true;

    // A control's own text is its label, and a heading or list item is page structure. Neither is a
    // message about a field.
    if (object.isControl() || object.isLink() || isStructural(object))
        return false;

    bool foundStaticText = false;
    return !containsNonMessageContent(object, foundStaticText, 0) && foundStaticText;
}

static bool isWorthAnnouncing(const String& text)
{
    if (text.isEmpty())
        return false;
    if (text.length() > maximumErrorMessageLength) {
        AXFORMLOG("Rejecting text of length "_s, text.length(), " as too long to be a validation message."_s);
        return false;
    }

    for (unsigned i = 0; i < text.length(); ++i) {
        char16_t character = text[i];
        if (!isPunctuation(character) && !isUnicodeWhitespace(character))
            return true;
    }
    return false;
}

static void appendStaticText(AXCoreObject& object, StringBuilder& builder, unsigned depth)
{
    if (depth > maximumErrorMessageDepth)
        return;

    auto children = object.unignoredChildren();
    for (const Ref<AXCoreObject>& child : children) {
        if (child->isStaticText()) {
            auto text = child->stringValue();
            if (text.isEmpty())
                continue;
            if (!builder.isEmpty())
                builder.append(' ');
            builder.append(text);
            continue;
        }
        appendStaticText(child.get(), builder, depth + 1);
    }
}

static String messageText(AXCoreObject& object)
{
    if (object.isStaticText())
        return object.stringValue();

    StringBuilder builder;
    appendStaticText(object, builder, 0);
    return builder.toString();
}

// Empty, or invalid by the page's own marking, the browser's validation, or an error detection already found.
static bool isEmptyOrInvalid(AccessibilityObject& field)
{
    if (field.isTextControl() && field.stringValue().isEmpty())
        return true;
    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(field.element()); select && select->value().isEmpty())
        return true;
    return field.invalidStatusIncludingInferred() != "false"_s;
}

// Close enough under a field to be about it, rather than about the part of the page around it.
static constexpr int maximumUncoloredMessageGap = 40;

static bool isDirectlyBelow(const IntRect& messageRect, const IntRect& fieldRect)
{
    // A message counts as below the field once it starts past the field's middle, not only past its bottom edge.
    // |fieldRect| and |messageRect| have been rounded upstream, and on a zoomed-out page, this rounding can actually
    // make these rects intersect a small amount. We still want to count that as being "directly below".
    return messageRect.y() >= fieldRect.center().y() && messageRect.y() - fieldRect.maxY() <= maximumUncoloredMessageGap;
}

// How far from a field a message may sit and still be about it, and what it costs to sit above the field
// rather than below, or outside the form rather than within it. Sites put the message under the input far
// more often than over it, and inside the form more often than after it, but all four happen.
static constexpr int maximumMessageGap = 200;
// Unrelated page content, such as a promotional banner, can sit well within the ordinary gap and still have nothing to
// do with the field it happens to line up with. Inside the form the layout vouches for the message, so only
// content outside it has to be adjacent.
static constexpr int maximumOutsideFormMessageGap = 60;
static constexpr int aboveFieldPenalty = 60;
static constexpr int outsideFormPenalty = 40;

static IntRect boundsForMessage(Element& element)
{
    auto rect = element.boundsInRootViewSpace();
    if (!rect.isEmpty())
        return rect;

    for (Ref descendant : descendantsOfType<Element>(element)) {
        auto descendantRect = descendant->boundsInRootViewSpace();
        if (!descendantRect.isEmpty())
            rect.unite(descendantRect);
    }
    return rect;
}

// How closely a message and a field line up, lower being closer, or nothing when the two do not line up
// at all. Horizontal overlap is required, so a message in one column of a two-column form cannot pair with
// the field beside it. Sitting above the field, or outside the form, both count against a message without
// ruling it out, as plenty of sites put the message above, and plenty put it after the </form>.
static std::optional<int> messageFieldAffinity(const IntRect& messageRect, const IntRect& fieldRect, bool messageIsInsideForm)
{
    if (messageRect.isEmpty() || fieldRect.isEmpty())
        return std::nullopt;
    if (std::min(fieldRect.maxX(), messageRect.maxX()) <= std::max(fieldRect.x(), messageRect.x()))
        return std::nullopt;

    int gap = 0;
    int penalty = messageIsInsideForm ? 0 : outsideFormPenalty;

    if (messageRect.y() >= fieldRect.maxY())
        gap = messageRect.y() - fieldRect.maxY();
    else if (fieldRect.y() >= messageRect.maxY()) {
        gap = fieldRect.y() - messageRect.maxY();
        penalty += aboveFieldPenalty;
    }

    // The cap is on the real distance. Penalties only order the candidates that are already close enough.
    if (gap > (messageIsInsideForm ? maximumMessageGap : maximumOutsideFormMessageGap))
        return std::nullopt;
    return gap + penalty;
}

AXFormActivityMonitor::AXFormActivityMonitor(AXObjectCache& cache)
    : m_cache(cache)
    , m_settleTimer(*this, &AXFormActivityMonitor::settleTimerFired)
{
}

AXFormActivityMonitor::~AXFormActivityMonitor() = default;

void AXFormActivityMonitor::setSettleDelayForTesting(std::optional<Seconds> delay)
{
    settleDelayForTesting = delay;
}

Seconds AXFormActivityMonitor::settleDelay() const
{
    return settleDelayForTesting.value_or(defaultSettleDelay);
}

// How often to look at what has arrived. Short relative to the periods below, so that the page going quiet
// is noticed promptly rather than only on a coarse boundary.
Seconds AXFormActivityMonitor::checkInterval() const
{
    return settleDelay() / 6;
}

// How long the page has to stay unchanged before what has been found counts as all there is.
Seconds AXFormActivityMonitor::quietPeriod() const
{
    return settleDelay() / 2;
}

void AXFormActivityMonitor::didAttemptSubmissionWithoutNavigation(Element& container, Element* submitter, Attempt attempt)
{
    if (m_container.get() == &container) {
        // Another attempt at the same fields. Keep what has been found, since those messages are already on screen and
        // would not be written again, and give the page a full window from now.
        if (attempt == Attempt::Submission)
            m_attempt = Attempt::Submission;
        captureRenderedFields(container);
        m_submitter = submitter;
        AXFORMLOG("Another attempt at the watched fields, so restarting the watch as a "_s, m_attempt == Attempt::Submission ? "submission"_s : "possible submission"_s,
            " and keeping its "_s, m_candidateErrorMessages.size(), " candidate messages."_s);
        startWatchWindow();
        return;
    }

    // A watch that has already found messages is left to report them, rather than lose them to something that may not
    // have been an attempt at all.
    if (attempt == Attempt::PossibleSubmission && isWatching() && !m_candidateErrorMessages.isEmpty()) {
        AXFORMLOG("Ignoring a possible submission elsewhere, since the running watch has already found "_s,
            m_candidateErrorMessages.size(), " candidate messages."_s);
        return;
    }

    AXFORMLOG("Watching for form errors after a "_s, attempt == Attempt::Submission ? "submission"_s : "possible submission"_s,
        is<HTMLFormElement>(container) ? " in a form"_s : " in a form-like container"_s, ", submitter "_s, !!submitter,
        ", settle delay "_s, settleDelay().milliseconds(), "ms."_s);
    m_container = container;
    m_submitter = submitter;
    m_attempt = attempt;
    m_candidateErrorMessages.clear();
    m_announcedText.clear();
    m_fieldsRenderedAtAttempt.clear();
    m_changedElements.clear();
    m_changedElementCount = 0;
    m_objectsVisited = 0;
    captureRenderedFields(container);
    startWatchWindow();
}

void AXFormActivityMonitor::captureRenderedFields(Element& container)
{
    for (Ref field : CheckedRef { m_cache }->fieldsForErrorPairing(container)) {
        if (field->renderer())
            m_fieldsRenderedAtAttempt.add(field);
    }
}

void AXFormActivityMonitor::startWatchWindow()
{
    m_reportIsPending = false;
    m_watchStartTime = MonotonicTime::now();
    m_lastChangeTime = m_watchStartTime;
    m_watchDeadline = m_watchStartTime + settleDelay() * maximumWatchMultiplier;
    m_settleTimer.startOneShot(checkInterval());
}

void AXFormActivityMonitor::didStartLoading(LocalFrame* frame)
{
    RefPtr container = m_container.get();
    if (!container)
        return;

    // Only a load in the frame the watched fields live in means the submission navigated after all.
    if (!frame || frame != container->document().frame())
        return;

    cancel();
}

void AXFormActivityMonitor::cancel()
{
    if (m_container) {
        AXFORMLOG("Cancelling the watch after a "_s, m_attempt == Attempt::Submission ? "submission, so clearing the errors detected in its fields."_s
            : "possible submission, so keeping any errors already detected."_s);
    }

    // A load that follows a submission means it went through, so the errors it had are resolved. A load that follows
    // anything else, like a click that starts a download, may leave the page and its errors exactly where they were.
    if (RefPtr container = m_container.get(); container && m_attempt == Attempt::Submission)
        CheckedRef { m_cache }->clearDetectedErrorsForContainer(*container);

    m_settleTimer.stop();
    m_container = nullptr;
    m_submitter = nullptr;
    m_reportIsPending = false;
    m_candidateErrorMessages.clear();
    m_announcedText.clear();
    m_fieldsRenderedAtAttempt.clear();
    m_changedElements.clear();
    m_changedElementCount = 0;
    m_objectsVisited = 0;
}

// The reds and oranges that sites give error text in both light and dark appearances, as opposed to the gray, black, blue
// or green of status, success and hint text. It stops short of the amber and yellow that warnings take.
static bool isErrorColor(const Color& color)
{
    constexpr float lowestErrorHueDegrees = 335;
    constexpr float highestErrorHueDegrees = 40;
    constexpr float minimumErrorSaturationPercent = 40;
    constexpr float minimumErrorLightnessPercent = 20;
    constexpr float maximumErrorLightnessPercent = 80;
    constexpr float minimumErrorOpacity = 0.5;

    auto hsla = color.toColorTypeLossy<HSLA<float>>().resolved();
    if (hsla.alpha < minimumErrorOpacity || hsla.saturation < minimumErrorSaturationPercent)
        return false;
    if (hsla.lightness < minimumErrorLightnessPercent || hsla.lightness > maximumErrorLightnessPercent)
        return false;
    return hsla.hue >= lowestErrorHueDegrees || hsla.hue <= highestErrorHueDegrees;
}

// The color a message's visible text is rendered in. Where it has several, like a red "Error:" ahead of the rest in black,
// an error color wins, since that is what marks the message as one.
static std::optional<Color> renderedTextColor(Element& message)
{
    std::optional<Color> firstColor;
    for (Ref node : composedTreeDescendants(message)) {
        RefPtr text = dynamicDowncast<Text>(node.get());
        if (!text || text->data().containsOnly<isASCIIWhitespace>())
            continue;

        CheckedPtr renderer = text->renderer();
        if (!renderer || isVisibilityHidden(renderer->style()))
            continue;

        auto color = protect(renderer->style())->visitedDependentColor();
        if (isErrorColor(color))
            return color;
        if (!firstColor)
            firstColor = color;
    }
    return firstColor;
}

static bool couldHoldMessage(Element& element)
{
    if (is<HTMLFormControlElement>(element) || element.isLink())
        return false;

    // Text inside a label names a control, not an error message.
    if (is<HTMLLabelElement>(element) || ancestorsOfType<HTMLLabelElement>(element).first())
        return false;

    if (element.isSVGElement())
        return false;

    return true;
}

void AXFormActivityMonitor::onChangedContent(Element& element)
{
    if (!isWatching() || !couldHoldMessage(element))
        return;

    m_lastChangeTime = MonotonicTime::now();
    if (m_changedElements.add(element).isNewEntry)
        ++m_changedElementCount;

    if (m_changedElementCount <= maximumChangedElements)
        return;
    // Compute the size because this is a WeakHashSet, and elements within could've been destroyed.
    m_changedElementCount = m_changedElements.computeSize();

    AXFORMLOG("Changed element cap reached at "_s, m_changedElementCount, ", evicting the oldest."_s);
    while (m_changedElementCount > maximumChangedElements && m_changedElements.tryTakeFirst())
        --m_changedElementCount;
}

void AXFormActivityMonitor::onChangedContent(AccessibilityObject& object)
{
    if (!isWatching())
        return;

    // Text nodes have no element of their own, so record the element holding the text, or the host of the shadow root holding it.
    RefPtr element = object.element();
    if (!element) {
        RefPtr node = object.node();
        element = node ? node->parentOrShadowHostElement() : nullptr;
    }
    if (element)
        onChangedContent(*element);
}

void AXFormActivityMonitor::collectErrorMessagesFrom(AccessibilityObject& object, unsigned depth)
{
    if (m_candidateErrorMessages.size() >= maximumCandidateErrorMessages)
        return;
    if (++m_objectsVisited > maximumObjectsVisited)
        return;
    if (object.isStaticTextLabel() || !object.labelForObjects().isEmpty())
        return;
    // Not a message by nature, and neither is anything inside it.
    if (object.isControl() || object.isLink() || isStructural(object))
        return;

    // An error message is text, or a container holding nothing but text and the icon sites like to put
    // beside it. What arrives here is often the container the message appeared in rather than the message
    // itself, so when this object is an container, look inside it for the message.
    if (!isErrorMessageShaped(object)) {
        if (depth >= maximumErrorMessageDepth)
            return;
        auto children = object.unignoredChildren();
        for (const Ref<AXCoreObject>& child : children) {
            if (RefPtr childObject = dynamicDowncast<AccessibilityObject>(child.get()))
                collectErrorMessagesFrom(*childObject, depth + 1);
        }
        return;
    }

    if (RefPtr element = object.element()) {
        // If there's already a browser-provided validation message, there's nothing more to gather.
        if (RefPtr listedElement = element->asValidatedFormListedElement(); listedElement && listedElement->isShowingValidationMessage())
            return;
    }

    String text = messageText(object).simplifyWhiteSpace(isASCIIWhitespace);
    if (!isWorthAnnouncing(text))
        return;

    // The element rather than the text node, both so it can be compared against the form's fields in
    // tree order and so it can act as the implicit aria-errormessage-like target.
    RefPtr element = object.element();
    if (!element) {
        RefPtr node = object.node();
        element = node ? node->parentOrShadowHostElement() : nullptr;
    }

    // A message and the wrapper around it can both arrive separately, as can a component and the parts of its shadow
    // root. Keep the outermost, which is the whole message, so a field does not end up with the same message attached twice.
    if (element) {
        for (auto& candidate : m_candidateErrorMessages) {
            RefPtr existing = candidate.element.get();
            if (!existing || !existing->isShadowIncludingInclusiveAncestorOf(*element))
                continue;
            // The same element arriving whole, after text inside it arrived on its own, is the whole message.
            if (existing == element && text.contains(candidate.text))
                candidate.text = WTF::move(text);
            return;
        }

        m_candidateErrorMessages.removeAllMatching([&] (const CandidateErrorMessage& candidate) {
            RefPtr existing = candidate.element.get();
            return existing && element->isShadowIncludingInclusiveAncestorOf(*existing);
        });
    }

    if (m_candidateErrorMessages.containsIf([&] (const CandidateErrorMessage& candidate) { return candidate.text == text; }))
        return;

    m_candidateErrorMessages.append(CandidateErrorMessage { WTF::move(text), element });
}

void AXFormActivityMonitor::onAnnouncedText(const String& text)
{
    if (!isWatching())
        return;

    auto announced = text.simplifyWhiteSpace(isASCIIWhitespace);
    if (announced.isEmpty() || m_announcedText.size() >= maximumAnnouncedText)
        return;

    m_announcedText.add(WTF::move(announced));
}

// Look at what has arrived since the last check. Answers how many changes there were to look at, so the
// caller can tell whether the page has stopped putting things on screen.
void AXFormActivityMonitor::collectErrorMessagesFromChangedElements()
{
    if (!isWatching())
        return;

    auto changedElements = std::exchange(m_changedElements, { });
    unsigned changedElementCount = std::exchange(m_changedElementCount, 0);
    if (!changedElementCount)
        return;

    // Everything below asks repeatedly whether an object is ignored, so start the cache.
    AXAttributeCacheScope attributeCacheScope(m_cache.ptr());

    for (Ref element : changedElements) {
        if (!element->isConnected())
            continue;
        RefPtr object = CheckedRef { m_cache }->getOrCreate(element.get());
        if (!object)
            continue;
        collectErrorMessagesFrom(*object, 0);
    }

    AXFORMLOG("Examined "_s, changedElementCount, " changed elements, now holding "_s,
        m_candidateErrorMessages.size(), " candidates, having visited "_s, m_objectsVisited, " objects of "_s,
        maximumObjectsVisited, " allowed."_s);
    if (m_objectsVisited > maximumObjectsVisited)
        AXFORMLOG("Stopped at the object cap, so later content was never examined."_s);
    if (m_candidateErrorMessages.size() >= maximumCandidateErrorMessages)
        AXFORMLOG("Reached the candidate cap, so later messages were dropped."_s);
}

void AXFormActivityMonitor::settleTimerFired()
{
    if (!m_container) {
        cancel();
        return;
    }

    auto now = MonotonicTime::now();
    bool haveSomethingToReport = !m_candidateErrorMessages.isEmpty();

    // Stop early once the page has gone quiet, and we have errors to report.
    bool pageWentQuiet = now - m_lastChangeTime >= quietPeriod();
    bool settleDelayElapsed = now >= m_watchStartTime + settleDelay();

    if (haveSomethingToReport && (pageWentQuiet || settleDelayElapsed)) {
        AXFORMLOG("Closing the watch after "_s, (now - m_watchStartTime).milliseconds(), "ms with "_s,
            m_candidateErrorMessages.size(), " candidates, because "_s,
            pageWentQuiet ? "the page went quiet."_s : "the settle delay elapsed."_s);
        requestReport();
        return;
    }

    // No error found yet. Keep looking in case a server is still being asked whether the entry was valid.
    if (now < m_watchDeadline) {
        m_settleTimer.startOneShot(checkInterval());
        return;
    }

    AXFORMLOG("Closing the watch at the outside deadline after "_s, (now - m_watchStartTime).milliseconds(),
        "ms with "_s, m_candidateErrorMessages.size(), " candidates."_s);
    requestReport();
}

// Reporting has to happen where layout is clean, so ask for a cache update and let it call report().
void AXFormActivityMonitor::requestReport()
{
    if (m_reportIsPending)
        return;
    m_reportIsPending = true;
    CheckedRef { m_cache }->scheduleCacheUpdate();
}

void AXFormActivityMonitor::report()
{
    m_reportIsPending = false;

    RefPtr container = m_container.get();
    RefPtr submitter = m_submitter.get();
    auto attempt = m_attempt;

    auto announced = std::exchange(m_announcedText, { });
    auto fieldsRenderedAtAttempt = std::exchange(m_fieldsRenderedAtAttempt, { });
    m_container = nullptr;
    m_submitter = nullptr;
    m_objectsVisited = 0;

    if (!container)
        return;
    CheckedRef { m_cache }->updateDetectedFormErrors();

    auto candidates = std::exchange(m_candidateErrorMessages, { });
    if (candidates.isEmpty())
        return;

    // Repair what the author left out. Each message is paired with the field it most likely belongs to, and
    // that field is given an aria-errormessage relation. Geometry is used to make the pairings, because
    // what makes a sighted user read a message as being about a field is that it appears next to it.
    auto fields = CheckedRef { m_cache }->fieldsForErrorPairing(*container);
    // What's known about each field, gathered the same way whatever the attempt was, so the decisions made from it sit in one place.
    struct FieldInfo {
        Ref<Element> field;
        IntRect rect;
        bool wasRenderedAtAttempt { false };
        // Unknown for a field that has no accessibility object to ask.
        std::optional<bool> isEmptyOrInvalid;
    };
    Vector<FieldInfo> fieldInfo;
    fieldInfo.reserveInitialCapacity(fields.size());
    for (auto& field : fields) {
        auto rect = field->boundsInRootViewSpace();
        if (rect.isEmpty())
            continue;

        bool wasRenderedAtAttempt = fieldsRenderedAtAttempt.contains(field.get());
        // Building an object changes what the error count and notifications below can reach, so build one only where the
        // uncolored-message rule may need the answer, and otherwise ask an object that already exists.
        RefPtr fieldObject = attempt == Attempt::PossibleSubmission && wasRenderedAtAttempt ? CheckedRef { m_cache }->getOrCreate(field.get()) : CheckedRef { m_cache }->get(field.ptr());
        std::optional<bool> isFieldEmptyOrInvalid;
        if (fieldObject)
            isFieldEmptyOrInvalid = isEmptyOrInvalid(*fieldObject);
        fieldInfo.append(FieldInfo { field, rect, wasRenderedAtAttempt, isFieldEmptyOrInvalid });
    }

    // Deliberately no message or field text in any of this logging. An error message can quote what the
    // user typed into the field, so only counts, lengths, indices, colors and geometry are recorded.
    AXFORMLOG("Container has "_s, fields.size(), " fields, "_s, fieldInfo.size(), " with a box."_s);
    if (shouldLogFormActivity()) {
        for (size_t fieldIndex = 0; fieldIndex < fieldInfo.size(); ++fieldIndex) {
            const auto& info = fieldInfo[fieldIndex];
            RefPtr fieldObject = CheckedRef { m_cache }->get(info.field.ptr());
            AXFORMLOG("  field "_s, fieldIndex, " role "_s, fieldObject ? roleToString(fieldObject->role()) : String { "none"_s },
                " bounds "_s, info.rect.x(), ","_s, info.rect.y(), " "_s, info.rect.width(), "x"_s, info.rect.height(),
                info.wasRenderedAtAttempt ? ", rendered at the attempt"_s : ", not rendered at the attempt"_s,
                !info.isEmptyOrInvalid ? ", no accessibility object"_s : (*info.isEmptyOrInvalid ? ", empty or invalid"_s : ", filled in and valid"_s));
        }
    }

    // One message belongs to one field.
    struct Pairing {
        size_t fieldIndex;
        size_t candidateIndex;
        int affinity;
    };
    // Measured once per message rather than once per pair, since for a message whose outermost element has
    // no box of its own this walks that element's descendants as well.
    struct MessageInfo {
        size_t candidateIndex;
        IntRect rect;
        bool isInsideForm;
        std::optional<Color> textColor;
        bool isErrorColored;
    };

    Vector<MessageInfo> messageInfo;
    messageInfo.reserveInitialCapacity(candidates.size());
    for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        auto& candidate = candidates[candidateIndex];
        RefPtr errorElement = candidate.element.get();
        if (!errorElement || !errorElement->isConnected())
            continue;

        // A page may reuse one element for a status and then an error, so take what the message says now.
        if (RefPtr errorObject = CheckedRef { m_cache }->get(*errorElement)) {
            auto currentText = messageText(*errorObject).simplifyWhiteSpace(isASCIIWhitespace);
            if (isWorthAnnouncing(currentText) && !currentText.contains(candidate.text))
                candidate.text = WTF::move(currentText);
        }
        auto textColor = renderedTextColor(*errorElement);
        bool isErrorColored = textColor && isErrorColor(*textColor);
        messageInfo.append(MessageInfo { candidateIndex, boundsForMessage(*errorElement), container->isShadowIncludingInclusiveAncestorOf(*errorElement), textColor, isErrorColored });
    }

    AXFORMLOG("Considering "_s, messageInfo.size(), " of "_s, candidates.size(), " candidate messages."_s);
    if (shouldLogFormActivity()) {
        for (const auto& message : messageInfo) {
            const auto& candidate = candidates[message.candidateIndex];
            AXFORMLOG("  candidate "_s, message.candidateIndex, " length "_s, candidate.text.length(),
                " insideForm "_s, message.isInsideForm, " bounds "_s, message.rect.x(), ","_s, message.rect.y(),
                " "_s, message.rect.width(), "x"_s, message.rect.height(),
                " color "_s, message.textColor ? serializationForCSS(*message.textColor) : String { "none"_s },
                message.isErrorColored ? ", an error color"_s : ", not an error color"_s);
        }
    }

    Vector<Pairing> pairings;
    for (const auto& message : messageInfo) {
        // Status and hint text has to be told apart from errors. In a 2FA form, "We sent a code to 123-456-7891" is not an error
        // for the code field. An unmistakable attempt to submit, or text in an error color, is evidence enough. Anything else
        // only counts right under the field nearest to it, if that field was already there when the user acted and they left
        // it empty or wrong.
        if (attempt == Attempt::Submission || message.isErrorColored) {
            AXFORMLOG("  candidate "_s, message.candidateIndex, " may pair with any field near it, "_s,
                attempt == Attempt::Submission ? "after a submission."_s : "because it is in an error color."_s);
            for (size_t fieldIndex = 0; fieldIndex < fieldInfo.size(); ++fieldIndex) {
                if (std::optional affinity = messageFieldAffinity(message.rect, fieldInfo[fieldIndex].rect, message.isInsideForm))
                    pairings.append(Pairing { fieldIndex, message.candidateIndex, *affinity });
            }
            continue;
        }

        std::optional<size_t> nearestFieldIndex;
        std::optional<int> nearestAffinity;
        for (size_t fieldIndex = 0; fieldIndex < fieldInfo.size(); ++fieldIndex) {
            std::optional affinity = messageFieldAffinity(message.rect, fieldInfo[fieldIndex].rect, message.isInsideForm);
            if (affinity && (!nearestAffinity || *affinity < *nearestAffinity)) {
                nearestFieldIndex = fieldIndex;
                nearestAffinity = affinity;
            }
        }
        if (!nearestFieldIndex) {
            AXFORMLOG("  candidate "_s, message.candidateIndex, " is uncolored after a possible submission, and near no field."_s);
            continue;
        }

        const auto& nearestField = fieldInfo[*nearestFieldIndex];
        bool isBelowNearestField = isDirectlyBelow(message.rect, nearestField.rect);
        bool isNearestFieldEmptyOrInvalid = nearestField.isEmptyOrInvalid.value_or(false);
        bool isAccepted = nearestField.wasRenderedAtAttempt && isNearestFieldEmptyOrInvalid && isBelowNearestField;
        AXFORMLOG("  candidate "_s, message.candidateIndex, " is uncolored after a possible submission, so "_s, isAccepted ? "accepting"_s : "rejecting"_s,
            " it. It is "_s, isBelowNearestField ? ""_s : "not "_s, "directly below its nearest field "_s, *nearestFieldIndex, ", which was "_s,
            nearestField.wasRenderedAtAttempt ? ""_s : "not "_s, "rendered at the attempt and is "_s, isNearestFieldEmptyOrInvalid ? ""_s : "not "_s,
            "empty or invalid."_s);
        if (isAccepted)
            pairings.append(Pairing { *nearestFieldIndex, message.candidateIndex, *nearestAffinity });
    }

    // Ties are broken by position so the same page always pairs the same way.
    std::ranges::sort(pairings, [] (const Pairing& a, const Pairing& b) {
        return std::tie(a.affinity, a.fieldIndex, a.candidateIndex) < std::tie(b.affinity, b.fieldIndex, b.candidateIndex);
    });

    Vector<bool> fieldTaken;
    fieldTaken.fill(false, fieldInfo.size());

    Vector<bool> candidateTaken;
    candidateTaken.fill(false, candidates.size());

    Vector<Pairing> acceptedPairings;
    for (const auto& pairing : pairings) {
        if (fieldTaken[pairing.fieldIndex] || candidateTaken[pairing.candidateIndex] || !candidates[pairing.candidateIndex].element)
            continue;

        fieldTaken[pairing.fieldIndex] = true;
        candidateTaken[pairing.candidateIndex] = true;
        acceptedPairings.append(pairing);

        AXFORMLOG("  paired candidate "_s, pairing.candidateIndex, " with field "_s, pairing.fieldIndex,
            ", affinity "_s, pairing.affinity);
    }

    if (shouldLogFormActivity()) {
        for (const auto& message : messageInfo) {
            if (!candidateTaken[message.candidateIndex])
                AXFORMLOG("  candidate "_s, message.candidateIndex, " paired with no field."_s);
        }
    }

    // Closeness decides which message goes with which field, but the user receives them in the order the fields come on the page.
    std::ranges::sort(acceptedPairings, [&] (const Pairing& a, const Pairing& b) {
        return is_lt(treeOrder<ComposedTree>(fieldInfo[a.fieldIndex].field.get(), fieldInfo[b.fieldIndex].field.get()));
    });

    Vector<AXObjectCache::DetectedFormErrorPairing> detectedErrors;
    Vector<String> pairedText;
    for (const auto& pairing : acceptedPairings) {
        RefPtr errorElement = candidates[pairing.candidateIndex].element.get();
        if (!errorElement)
            continue;

        detectedErrors.append({ .field = fieldInfo[pairing.fieldIndex].field, .message = errorElement.releaseNonNull() });
        const auto& text = candidates[pairing.candidateIndex].text;
        if (!pairedText.contains(text))
            pairedText.append(text);
    }

    bool detectedAnyError = !detectedErrors.isEmpty();
    RefPtr<Element> firstPairedField = detectedAnyError ? detectedErrors[0].field.ptr() : nullptr;
    if (detectedAnyError)
        CheckedRef { m_cache }->addDetectedFormErrors(WTF::move(detectedErrors));

    // This class repairs forms in multiple ways -- the first of which is pairing fields and error messages,
    // aria-errormessage style. The second is announcing errors that appear (if they were not already done so
    // via role=alert or similar). Avoid announcing error messages that were already announced.
    Vector<String> unannouncedText;
    for (const auto& text : pairedText) {
        // Deliberately not an equality test, since an announcement folds in the name of the icon beside the
        // message (if present) and so the spoken string is longer than the message itself.
        bool alreadyReceived = false;
        for (const auto& announcedText : announced) {
            if (announcedText.contains(text) || text.contains(announcedText)) {
                AXFORMLOG("  a message of length "_s, text.length(), " was already announced, so not repeating it."_s);
                alreadyReceived = true;
                break;
            }
        }

        if (alreadyReceived)
            continue;
        unannouncedText.append(text);
    }

    AXFORMLOG("Reporting "_s, unannouncedText.size(), " unannounced of "_s, pairedText.size(),
        " paired messages, "_s, announced.size(), " already announced, detectedAnyError "_s, detectedAnyError);

    if (unannouncedText.isEmpty() && !detectedAnyError) {
        AXFORMLOG("Posting nothing, since every message was already announced and no error was detected."_s);
        return;
    }

    unsigned errorFieldCount = 0;
    for (auto& field : fields) {
        RefPtr fieldObject = CheckedRef { m_cache }->getOrCreate(field.get());
        if (fieldObject && !fieldObject->isIgnored() && fieldObject->invalidStatusIncludingInferred() != "false"_s)
            ++errorFieldCount;
    }

    // Post on the control the user activated, falling back to the form itself for an implicit submission, and to a
    // field that was paired when the page has replaced the control while it checked the fields.
    RefPtr<AccessibilityObject> target;
    for (RefPtr element : { submitter, container, firstPairedField }) {
        target = element ? CheckedRef { m_cache }->getOrCreate(*element) : nullptr;
        if (target)
            break;
    }
    if (!target)
        return;

    // Pressing Enter in a text field is an attempt too, but that field is not a button to come back to.
    RefPtr submitterInput = dynamicDowncast<HTMLInputElement>(submitter.get());
    bool targetIsSubmitter = submitter && submitter == target->element() && !(submitterInput && submitterInput->isTextField());

    AXFORMLOG("Posting PossibleFormValidationError with errorFieldCount "_s, errorFieldCount, targetIsSubmitter ? ", on the button the user pressed."_s : "."_s);
    CheckedRef { m_cache }->postPossibleFormValidationErrorNotification(*target, WTF::move(unannouncedText), errorFieldCount, targetIsSubmitter);
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

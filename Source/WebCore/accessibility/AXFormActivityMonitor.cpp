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
#include "AXObjectCacheInlines.h"
#include "AccessibilityObject.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementInlines.h"
#include "HTMLBodyElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNames.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "ValidatedFormListedElement.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXFormActivityMonitor);

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
    if (text.isEmpty() || text.length() > maximumErrorMessageLength)
        return false;

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

struct FieldBounds {
    Ref<Element> field;
    IntRect rect;
};

// How far from a field a message may sit and still be about it, and what it costs to sit above the field
// rather than below, or outside the form rather than within it. Sites put the message under the input far
// more often than over it, and inside the form more often than after it, but all four happen.
static constexpr int maximumMessageGap = 200;
// Page furniture such as a promotional banner can sit well within the ordinary gap and still have nothing to
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

void AXFormActivityMonitor::didAttemptSubmissionWithoutNavigation(HTMLFormElement& form, HTMLFormControlElement* submitter)
{
    m_form = form;
    m_submitter = submitter;
    m_candidateErrorMessages.clear();
    m_announcedText.clear();
    m_changedElements.clear();
    m_changedElementCount = 0;
    m_objectsVisited = 0;
    m_watchStartTime = MonotonicTime::now();
    m_lastChangeTime = m_watchStartTime;
    m_watchDeadline = m_watchStartTime + settleDelay() * maximumWatchMultiplier;
    m_settleTimer.startOneShot(checkInterval());
}

void AXFormActivityMonitor::didStartLoading(LocalFrame* frame)
{
    RefPtr form = m_form.get();
    if (!form)
        return;

    // Only a load in the frame the watched form lives in means the submission navigated after all.
    if (!frame || frame != form->document().frame())
        return;

    cancel();
}

void AXFormActivityMonitor::cancel()
{
    if (RefPtr form = m_form.get())
        CheckedRef { m_cache }->clearDetectedErrorsForForm(*form);

    m_settleTimer.stop();
    m_form = nullptr;
    m_submitter = nullptr;
    m_reportIsPending = false;
    m_candidateErrorMessages.clear();
    m_announcedText.clear();
    m_changedElements.clear();
    m_changedElementCount = 0;
    m_objectsVisited = 0;
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

void AXFormActivityMonitor::noteChangedContent(Element& element)
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

    while (m_changedElementCount > maximumChangedElements && m_changedElements.tryTakeFirst())
        --m_changedElementCount;
}

void AXFormActivityMonitor::noteChangedContent(AccessibilityObject& object)
{
    if (!isWatching())
        return;

    // Text nodes have no element of their own, so record the element holding the text.
    RefPtr element = object.element();
    if (!element) {
        RefPtr node = object.node();
        element = node ? node->parentElement() : nullptr;
    }
    if (element)
        noteChangedContent(*element);
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
        element = node ? node->parentElement() : nullptr;
    }

    // A message and the wrapper around it can both arrive separately. Keep the outermost, which is the
    // whole message, so a field does not end up with the same message attached twice.
    if (element) {
        for (const auto& candidate : m_candidateErrorMessages) {
            RefPtr existing = candidate.element.get();
            if (existing && existing->contains(*element))
                return;
        }

        m_candidateErrorMessages.removeAllMatching([&] (const CandidateErrorMessage& candidate) {
            RefPtr existing = candidate.element.get();
            return existing && element->contains(*existing);
        });
    }

    if (m_candidateErrorMessages.containsIf([&] (const CandidateErrorMessage& candidate) { return candidate.text == text; }))
        return;

    m_candidateErrorMessages.append(CandidateErrorMessage { WTF::move(text), element });
}

void AXFormActivityMonitor::noteAnnouncedText(const String& text)
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
}

void AXFormActivityMonitor::settleTimerFired()
{
    if (!m_form) {
        cancel();
        return;
    }

    auto now = MonotonicTime::now();
    bool haveSomethingToReport = !m_candidateErrorMessages.isEmpty();

    // Stop early once the page has gone quiet, and we have errors to report.
    bool pageWentQuiet = now - m_lastChangeTime >= quietPeriod();
    bool settleDelayElapsed = now >= m_watchStartTime + settleDelay();

    if (haveSomethingToReport && (pageWentQuiet || settleDelayElapsed)) {
        requestReport();
        return;
    }

    // No error found yet. Keep looking in case a server is still being asked whether the entry was valid.
    if (now < m_watchDeadline) {
        m_settleTimer.startOneShot(checkInterval());
        return;
    }

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

    RefPtr form = m_form.get();
    RefPtr submitter = m_submitter.get();

    auto announced = std::exchange(m_announcedText, { });
    m_form = nullptr;
    m_submitter = nullptr;
    m_objectsVisited = 0;

    if (!form)
        return;
    CheckedRef { m_cache }->updateDetectedFormErrors();

    auto candidates = std::exchange(m_candidateErrorMessages, { });
    if (candidates.isEmpty())
        return;

    // Repair what the author left out. Each message is paired with the field it most likely belongs to, and
    // that field is given an aria-errormessage relation. Geometry is used to make the pairings, because
    // what makes a sighted user read a message as being about a field is that it appears next to it.
    auto fields = CheckedRef { m_cache }->formFieldsForErrorPairing(*form);
    Vector<FieldBounds> fieldBounds;
    fieldBounds.reserveInitialCapacity(fields.size());
    for (auto& field : fields) {
        auto rect = field->boundsInRootViewSpace();
        if (!rect.isEmpty())
            fieldBounds.append(FieldBounds { field, rect });
    }

    // One message belongs to one field.
    struct Pairing {
        size_t fieldIndex;
        size_t candidateIndex;
        int affinity;
    };
    // Measured once per message rather than once per pair, since for a message whose outermost element has
    // no box of its own this walks that element's descendants as well.
    struct MessageBounds {
        size_t candidateIndex;
        IntRect rect;
        bool isInsideForm;
    };

    Vector<MessageBounds> messageBounds;
    messageBounds.reserveInitialCapacity(candidates.size());
    for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        RefPtr errorElement = candidates[candidateIndex].element.get();
        if (!errorElement || !errorElement->isConnected())
            continue;
        messageBounds.append(MessageBounds { candidateIndex, boundsForMessage(*errorElement), form->contains(*errorElement) });
    }

    Vector<Pairing> pairings;
    for (size_t fieldIndex = 0; fieldIndex < fieldBounds.size(); ++fieldIndex) {
        for (const auto& message : messageBounds) {
            std::optional affinity = messageFieldAffinity(message.rect, fieldBounds[fieldIndex].rect, message.isInsideForm);
            if (affinity)
                pairings.append(Pairing { fieldIndex, message.candidateIndex, *affinity });
        }
    }

    // Ties are broken by position so the same page always pairs the same way.
    std::ranges::sort(pairings, [] (const Pairing& a, const Pairing& b) {
        return std::tie(a.affinity, a.fieldIndex, a.candidateIndex) < std::tie(b.affinity, b.fieldIndex, b.candidateIndex);
    });

    Vector<bool> fieldTaken;
    fieldTaken.fill(false, fieldBounds.size());

    Vector<bool> candidateTaken;
    candidateTaken.fill(false, candidates.size());

    Vector<AXObjectCache::DetectedFormErrorPairing> detectedErrors;
    Vector<String> pairedText;
    for (const auto& pairing : pairings) {
        if (fieldTaken[pairing.fieldIndex] || candidateTaken[pairing.candidateIndex])
            continue;

        RefPtr errorElement = candidates[pairing.candidateIndex].element.get();
        if (!errorElement)
            continue;

        fieldTaken[pairing.fieldIndex] = true;
        candidateTaken[pairing.candidateIndex] = true;

        auto& field = fieldBounds[pairing.fieldIndex].field;
        const auto& text = candidates[pairing.candidateIndex].text;
        detectedErrors.append({ .field = field, .message = errorElement.releaseNonNull() });
        if (!pairedText.contains(text))
            pairedText.append(text);
    }

    bool detectedAnyError = !detectedErrors.isEmpty();
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
                alreadyReceived = true;
                break;
            }
        }

        if (alreadyReceived)
            continue;
        unannouncedText.append(text);
    }

    if (unannouncedText.isEmpty() && !detectedAnyError)
        return;

    unsigned errorFieldCount = 0;
    for (auto& field : fields) {
        RefPtr fieldObject = CheckedRef { m_cache }->get(field.ptr());
        if (fieldObject && fieldObject->invalidStatus() != "false"_s)
            ++errorFieldCount;
    }

    // Post on the control the user activated so the assistive technology can reach the rest of the
    // form through AXFormOwner, falling back to the form itself for an implicit submission.
    Ref<Element> targetElement = submitter ? static_cast<Element&>(*submitter) : static_cast<Element&>(*form);
    RefPtr target = CheckedRef { m_cache }->getOrCreate(targetElement.get());
    if (!target)
        return;

    CheckedRef { m_cache }->postPossibleFormValidationErrorNotification(*target, WTF::move(unannouncedText), errorFieldCount);
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

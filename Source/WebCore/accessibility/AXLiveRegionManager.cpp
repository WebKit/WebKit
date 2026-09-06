/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "AXLiveRegionManager.h"

#if PLATFORM(COCOA)

#include "AXNotifications.h"
#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CharacterProperties.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXLiveRegionManager);

#if PLATFORM(MAC)
static constexpr ASCIILiteral accessibilityLanguageAttributeKey = "AXLanguage"_s;
static constexpr ASCIILiteral accessibilityIsLiveRegionRemovalAttributeKey = "AXIsLiveRegionRemoval"_s;
#else
static constexpr ASCIILiteral accessibilityLanguageAttributeKey = "UIAccessibilitySpeechAttributeLanguage"_s;
static constexpr ASCIILiteral accessibilityIsLiveRegionRemovalAttributeKey = "UIAccessibilityTokenIsLiveRegionRemoval"_s;
#endif

struct LiveRegionObjectMetadata {
    String text;
    String language;
    HashSet<AXID> descendants;
};

AXLiveRegionManager::AXLiveRegionManager(AXObjectCache& cache)
    : m_cache(cache)
    , m_emptyRegionSettleTimer(*this, &AXLiveRegionManager::emptyRegionSettleTimerFired)
{
}

static UNUSED_FUNCTION String debugDescriptionForSnapshot(LiveRegionSnapshot snapshot)
{
    StringBuilder result;
    result.append("SNAPSHOT:\n"_s);
    result.append("\tStatus: "_s);

    switch (snapshot.liveRegionStatus) {
    case LiveRegionStatus::Off:
        result.append("Off"_s);
        break;
    case LiveRegionStatus::Polite:
        result.append("Polite"_s);
        break;
    case LiveRegionStatus::Assertive:
        result.append("Assertive"_s);
        break;
    }
    result.append('\n');

    result.append("\tRelevant: "_s);
    if (snapshot.liveRegionRelevant.isEmpty())
        result.append("(default: additions text)"_s);
    else {
        bool isFirst = true;
        if (snapshot.liveRegionRelevant.contains(LiveRegionRelevant::Additions)) {
            result.append("additions"_s);
            isFirst = false;
        }
        if (snapshot.liveRegionRelevant.contains(LiveRegionRelevant::Removals)) {
            if (!isFirst)
                result.append(' ');
            result.append("removals"_s);
            isFirst = false;
        }
        if (snapshot.liveRegionRelevant.contains(LiveRegionRelevant::Text)) {
            if (!isFirst)
                result.append(' ');
            result.append("text"_s);
            isFirst = false;
        }
        if (snapshot.liveRegionRelevant.contains(LiveRegionRelevant::All)) {
            if (!isFirst)
                result.append(' ');
            result.append("all"_s);
        }
    }
    result.append('\n');

    result.append("\tObjects: "_s);
    result.append(snapshot.objects.size());
    result.append('\n');

    for (size_t i = 0; i < snapshot.objects.size(); ++i) {
        const auto& object = snapshot.objects[i];
        result.append("\t\t["_s);
        result.append(i);
        result.append("] AXID="_s);
        result.append(object.objectID.loggingString());
        result.append(" text=\""_s);
        result.append(object.text);
        result.append("\"\n"_s);
    }

    return result.toString();
}

void AXLiveRegionManager::registerLiveRegion(AccessibilityObject& object, bool speakIfNecessary)
{
    m_liveRegions.set(object.objectID(), buildLiveRegionSnapshot(object));
    // Alerts should speak when added to the page (or initialized for the first time), unlike all other live regions.
    bool isAlertOrAlertDialog = speakIfNecessary && (object.role() == AccessibilityRole::ApplicationAlert || object.role() == AccessibilityRole::ApplicationAlertDialog);
    if (isAlertOrAlertDialog)
        handleLiveRegionChange(object, AnnouncementContents::All);
}

static LiveRegionStatus stringToLiveRegionStatus(const String& string)
{
    if (equalLettersIgnoringASCIICase(string, "assertive"_s))
        return LiveRegionStatus::Assertive;
    if (equalLettersIgnoringASCIICase(string, "polite"_s))
        return LiveRegionStatus::Polite;

    return LiveRegionStatus::Off;
}

static OptionSet<LiveRegionRelevant> stringToLiveRegionRelevant(const String& string)
{
    OptionSet<LiveRegionRelevant> result;
    for (auto attribute : StringView(string).split(' ')) {
        if (equalLettersIgnoringASCIICase(attribute, "additions"_s))
            result.add(LiveRegionRelevant::Additions);
        else if (equalLettersIgnoringASCIICase(attribute, "all"_s))
            result.add(LiveRegionRelevant::All);
        else if (equalLettersIgnoringASCIICase(attribute, "removals"_s))
            result.add(LiveRegionRelevant::Removals);
        else if (equalLettersIgnoringASCIICase(attribute, "text"_s))
            result.add(LiveRegionRelevant::Text);
    }
    return result;
}

// How long a live region has to stay empty before the clear is treated as something the page meant,
// rather than the gap between tearing the region down and rebuilding it.
static constexpr Seconds emptyRegionSettleDelay { 100_ms };

// Whether two snapshots hold the same text laid out the same way.
static bool hasSameObjectTexts(const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot)
{
    if (oldSnapshot.objects.size() != newSnapshot.objects.size())
        return false;

    for (size_t i = 0; i < oldSnapshot.objects.size(); ++i) {
        if (oldSnapshot.objects[i].text != newSnapshot.objects[i].text)
            return false;
    }
    return true;
}

void AXLiveRegionManager::handleLiveRegionChange(AccessibilityObject& object, AnnouncementContents contents)
{
    // If this is a new live region, don't speak it upon registering.
    auto iterator = m_liveRegions.find(object.objectID());
    if (iterator == m_liveRegions.end()) {
        registerLiveRegion(object);
        return;
    }

    LiveRegionSnapshot oldSnapshot = contents == AnnouncementContents::All ? LiveRegionSnapshot { } : iterator->value;
    bool baselineHadText = iterator->value.hasAnyText;
    bool baselineSettledEmpty = iterator->value.settledEmpty;

    LiveRegionSnapshot newSnapshot = buildLiveRegionSnapshot(object);

    // Streaming pages often re-render a region by emptying it and refilling it in a later task, so this
    // empty state is transient rather than a change. Adopting it as the baseline would make the refilled
    // text look entirely new, so instead, only keep the last baseline that had text.
    if (!newSnapshot.hasAnyText && baselineHadText) {
        m_regionsPendingEmpty.add(object.objectID());
        if (!m_emptyRegionSettleTimer.isActive())
            m_emptyRegionSettleTimer.startOneShot(emptyRegionSettleDelay);

        // The baseline is kept, but a region that asked to hear removals still has to hear that its
        // content went away.
        if (newSnapshot.liveRegionRelevant.containsAny({ LiveRegionRelevant::Removals, LiveRegionRelevant::All })) {
            // Re-find rather than reusing the iterator, which building the snapshot above may have invalidated.
            auto baseline = m_liveRegions.find(object.objectID());
            if (baseline != m_liveRegions.end() && !baseline->value.announcedEmptyRemoval) {
                baseline->value.announcedEmptyRemoval = true;
                postAnnouncementForChange(object, oldSnapshot, newSnapshot);
            }
        }
        return;
    }

    // Content is present, so whatever empty state was being waited on was only part of a render.
    m_regionsPendingEmpty.remove(object.objectID());

    // The region was cleared, stayed cleared, and has now come back saying exactly what it said before,
    // split across objects in the same way. Reset the old snapshot so an announcement can be made.
    if (baselineSettledEmpty && hasSameObjectTexts(oldSnapshot, newSnapshot))
        oldSnapshot = LiveRegionSnapshot { };

    // Re-find rather than reusing the iterator, which building the snapshot above may have invalidated.
    m_liveRegions.set(object.objectID(), newSnapshot);

    postAnnouncementForChange(object, oldSnapshot, newSnapshot);
}

// Nothing came back while the timer ran, so these regions were cleared rather than caught mid-render.
void AXLiveRegionManager::emptyRegionSettleTimerFired()
{
    for (auto axID : std::exchange(m_regionsPendingEmpty, { })) {
        if (auto iterator = m_liveRegions.find(axID); iterator != m_liveRegions.end())
            iterator->value.settledEmpty = true;
    }
}

// Cap on the aggregated text, so that content made of a few enormous nodes cannot overflow the builder
// or make comparison arbitrarily expensive. Exceeding it truncates the snapshot rather than failing.
static constexpr size_t maximumAggregateLength = 131072;

// Concatenates the objects' text, separated by a single space, with each object's own whitespace runs
// collapsed and its leading and trailing whitespace dropped.
static String aggregateText(const Vector<LiveRegionObject>& objects, bool& isTruncated, Vector<size_t>* objectEndOffsets = nullptr)
{
    size_t lengthHint = 0;
    for (auto& object : objects)
        lengthHint += object.text.length() + 1;

    StringBuilder builder;
    builder.reserveCapacity(std::min(lengthHint, maximumAggregateLength + 1));
    if (objectEndOffsets)
        objectEndOffsets->reserveInitialCapacity(objects.size());

    for (auto& object : objects) {
        String text = object.text.simplifyWhiteSpace(isUnicodeWhitespace);
        if (!text.isEmpty()) {
            if (builder.length())
                builder.append(' ');
            builder.append(text);
        }

        if (builder.length() > maximumAggregateLength) {
            isTruncated = true;
            return { };
        }

        if (objectEndOffsets)
            objectEndOffsets->append(builder.length());
    }
    return builder.toString();
}

// True when text carries no word of its own, which is not worth announcing. Resolving inline markup can
// split a trailing punctuation mark into its own object, which would otherwise be announced as just ".".
static bool isPunctuationOnly(const String& text)
{
    for (unsigned i = 0; i < text.length(); ++i) {
        char16_t character = text[i];
        if (!isPunctuation(character) && !isUnicodeWhitespace(character))
            return false;
    }
    return true;
}

// Limit on the number of objects visited during snapshot building to prevent
// web content from hanging the process with excessively large live regions.
static constexpr size_t maximumSnapshotObjects = 512;

LiveRegionSnapshot AXLiveRegionManager::buildLiveRegionSnapshot(AccessibilityObject& object) const
{
    ++m_snapshotBuildCount;

    LiveRegionSnapshot snapshot;
    snapshot.liveRegionStatus = stringToLiveRegionStatus(object.liveRegionStatus());
    snapshot.liveRegionRelevant = stringToLiveRegionRelevant(object.liveRegionRelevant());

    size_t objectsVisited = 0;
    std::function<void(AccessibilityObject&)> buildObjectList = [protectedThis = CheckedRef { *this }, &buildObjectList, &snapshot, &objectsVisited] (AccessibilityObject& object) {
        if (objectsVisited >= maximumSnapshotObjects) {
            snapshot.isTruncated = true;
            return;
        }
        ++objectsVisited;

        // Treat atomic objects as one object, so when they change the entire subtree is announced.
        if (object.liveRegionAtomic()) {
            HashSet<AXID> descendants;

            // Collect all atomic-region descendants to detect when nodes are added/removed within the atomic region.
            std::function<void(AccessibilityObject&)> collectDescendants = [&collectDescendants, &descendants, &objectsVisited, &snapshot] (AccessibilityObject& descendant) {
                if (objectsVisited >= maximumSnapshotObjects) {
                    snapshot.isTruncated = true;
                    return;
                }
                ++objectsVisited;

                descendants.add(descendant.objectID());
                for (auto& child : descendant.unignoredChildren())
                    collectDescendants(downcast<AccessibilityObject>(child.get()));
            };

            for (auto& child : object.unignoredChildren())
                collectDescendants(downcast<AccessibilityObject>(child.get()));

            String text = object.announcementText();
            snapshot.hasAnyText |= !text.isEmpty();
            snapshot.hasAtomicRegion = true;
            snapshot.objects.append({ object.objectID(), WTF::move(text), object.languageIncludingAncestors(), WTF::move(descendants), /* isTextContent */ false });
            return;
        }

        if (protectedThis->shouldIncludeInSnapshot(object)) {
            String text = object.announcementText();
            snapshot.hasAnyText |= !text.isEmpty();
            snapshot.objects.append({ object.objectID(), WTF::move(text), object.languageIncludingAncestors(), { }, object.isStaticText() });
        } else {
            for (auto& child : object.unignoredChildren())
                buildObjectList(downcast<AccessibilityObject>(child.get()));
        }
    };

    buildObjectList(object);

    if (!snapshot.isTruncated)
        snapshot.aggregatedText = aggregateText(snapshot.objects, snapshot.isTruncated, &snapshot.objectEndOffsets);

    return snapshot;
}

bool AXLiveRegionManager::shouldIncludeInSnapshot(AccessibilityObject& object) const
{
    if (object.isStaticText())
        return true;

    // Description will account for alt text, aria-label(ledby), and title attributes.
    if (String description = object.description(); description.length())
        return true;

    // If an object has unignored children, there isn't a need to include it in the snapshot since the children will return YES.
    if (object.hasUnignoredChild())
        return false;

    // For leaf objects, include if they have a value (e.g., form controls).
    if (!object.stringValue().isEmpty())
        return true;

    Vector<AccessibilityText> accessibilityText;
    object.accessibilityText(accessibilityText);

#if PLATFORM(COCOA)
    // For leaf objects, include if they have accessible description text (e.g., images with alt text).
    if (!object.descriptionAttributeValue(&accessibilityText).isEmpty())
        return true;
#endif

    // Some leaf objects (like buttons) return their text via `title`.
    if (!object.title(&accessibilityText).isEmpty())
        return true;

    return false;
}

// Returns the objects carrying text the region did not have before, which is empty when its text did
// not change at all. Streaming pages re-render their whole live region as each chunk arrives, which
// spreads already-announced text across a different set of objects. For example, a paragraph that splits
// into three once a bold span or citation resolves, and once the response is complete the many small nodes
// are coalesced into a few large ones. Comparing objects individually sees all of those as brand new and
// re-announces the whole response, so the region's combined text is what has to be compared.
//
// Returns std::nullopt when the text changed in some way other than growing at the end, or when either snapshot
// is too incomplete to compare.
static std::optional<Vector<LiveRegionObject>> appendedObjects(const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot)
{
    // A truncated snapshot's text can stop growing while the region keeps growing, which would read as
    // "nothing changed" and silence the rest of the content. Fall back to comparing objects instead.
    if (oldSnapshot.isTruncated || newSnapshot.isTruncated)
        return std::nullopt;

    // Atomic regions are announced whole, so they must not be reduced to their appended text.
    if (oldSnapshot.hasAtomicRegion || newSnapshot.hasAtomicRegion)
        return std::nullopt;

    const String& oldAggregate = oldSnapshot.aggregatedText;
    const String& newAggregate = newSnapshot.aggregatedText;

    if (oldAggregate.isEmpty())
        return std::nullopt;

    // The same text spread across a different set of objects. Streaming pages coalesce their many
    // small nodes into fewer, larger ones once the response is complete, which leaves the text
    // unchanged but matches up with none of the previous objects. Nothing was added, so there is
    // nothing to announce.
    if (newAggregate == oldAggregate)
        return Vector<LiveRegionObject> { };

    if (!newAggregate.startsWith(oldAggregate))
        return std::nullopt;

    // Streaming often delivers partial words, so the point where the old text ended can fall in the
    // middle of a word. Back up to the start of that word and announce it whole rather than announcing a
    // fragment of it.
    static constexpr size_t maximumWordBoundaryBackup = 32;
    // A region whose entire text is this short is treated as a single value, such as a counter, where
    // the new value should be announced in full rather than only the characters that changed.
    static constexpr size_t maximumSingleValueLength = 8;

    size_t splitOffset = oldAggregate.length();
    if (!isUnicodeWhitespace(newAggregate[splitOffset])) {
        size_t limit = splitOffset > maximumWordBoundaryBackup ? splitOffset - maximumWordBoundaryBackup : 0;
        size_t candidate = splitOffset;
        while (candidate > limit && !isUnicodeWhitespace(newAggregate[candidate - 1]))
            --candidate;

        if (candidate && isUnicodeWhitespace(newAggregate[candidate - 1]))
            splitOffset = candidate;
        else if (oldAggregate.length() <= maximumSingleValueLength) {
            // No word boundary was found. Scripts that do not separate words with spaces (Chinese,
            // Japanese, Thai) never have one, so only rewind the whole text when it is short enough to
            // plausibly be a single value. Otherwise the entire region would be re-announced per chunk.
            splitOffset = 0;
        }
    }

    // Offsets were recorded alongside the text when the snapshot was built. They only cover every object
    // when the walk ran to completion, which the truncation check above has already established.
    const Vector<size_t>& objectEndOffsets = newSnapshot.objectEndOffsets;
    ASSERT(objectEndOffsets.size() == newSnapshot.objects.size());

    Vector<LiveRegionObject> appended;
    size_t objectStart = 0;
    for (size_t i = 0; i < newSnapshot.objects.size(); ++i) {
        size_t objectEnd = objectEndOffsets[i];
        size_t contributionStart = std::exchange(objectStart, objectEnd);

        // Skip objects that contributed nothing, and those whose text was entirely announced already.
        if (objectEnd <= contributionStart || objectEnd <= splitOffset)
            continue;

        LiveRegionObject object = newSnapshot.objects[i];
        size_t announceFrom = contributionStart;
        if (contributionStart < splitOffset) {
            // Only text content can be cut mid-object. Anything else is a single accessible name or
            // value (a button's title, an image's alt text), where announcing part of one would say
            // something like "over" for a button relabelled "Start over", so those are announced whole.
            if (object.isTextContent)
                announceFrom = splitOffset;
        }

        // Take the text from the aggregate, which is already whitespace-collapsed.
        object.text = newAggregate.substring(announceFrom, objectEnd - announceFrom).trim(isUnicodeWhitespace);
        if (object.text.isEmpty() || isPunctuationOnly(object.text))
            continue;

        appended.append(WTF::move(object));
    }

    return appended;
}

AXLiveRegionManager::LiveRegionDiff AXLiveRegionManager::computeChanges(const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot) const
{
    // Here we compare the old and new live region to compute:
    // - Additions: New objects, or atomic regions where nodes were added AND text changed.
    // - Deletions: Objects that were removed from the region, or atomic regions where nodes were removed AND text changed.
    // - Changes: Text content/values that changed between the same object (without node additions/removals).

    const Vector<LiveRegionObject>& oldObjects = oldSnapshot.objects;
    const Vector<LiveRegionObject>& newObjects = newSnapshot.objects;

    LiveRegionDiff diff;

    if (auto appended = appendedObjects(oldSnapshot, newSnapshot)) {
        // The region's text only grew at the end, so announce just the new text. This has to be checked
        // before comparing objects individually, because a re-render can spread the same text across a
        // different set of objects, which object-level comparison sees as entirely new content.
        // Text that grew on an object that was already there is a text change. Text on an object that
        // was not there is an addition. aria-relevant distinguishes the two, so a region asking only for
        // "additions" must not hear text edits, and one asking only for "text" must still hear them.
        for (auto& object : *appended) {
            bool objectExistedBefore = oldObjects.containsIf([&] (const auto& oldObject) {
                return oldObject.objectID == object.objectID;
            });
            if (objectExistedBefore)
                diff.changed.append(object);
            else
                diff.added.append(object);
        }
        return diff;
    }

    // Build a map of old objects for lookup. As we match them with new objects, we'll remove them.
    // Whatever remains unmatched at the end represents removals.
    HashMap<AXID, LiveRegionObjectMetadata> unmatchedOldObjects;
    unmatchedOldObjects.reserveInitialCapacity(oldObjects.size());

    // Index the old objects by text too, so content that survived a re-render can still be matched
    // even though the objects backing it are new. See the second pass below.
    HashMap<String, Vector<AXID>> oldObjectIDsByText;

    for (auto& object : oldObjects) {
        unmatchedOldObjects.set(object.objectID, LiveRegionObjectMetadata { object.text, object.language, object.descendants });
        if (!object.text.isEmpty())
            oldObjectIDsByText.add(object.text, Vector<AXID> { }).iterator->value.append(object.objectID);
    }

    // Additions are collected with the index of the object that produced them, so that the two passes
    // below can be merged back into document order before anything is announced.
    Vector<std::pair<size_t, LiveRegionObject>> additions;

    // First pass. Try to match by AXID. This has to finish before any text matching happens below, so that
    // objects which still exist are always paired with themselves rather than consumed by a text match.
    Vector<size_t> unmatchedNewObjectIndices;
    for (size_t i = 0; i < newObjects.size(); ++i) {
        const auto& newObject = newObjects[i];
        auto iterator = unmatchedOldObjects.find(newObject.objectID);
        if (iterator == unmatchedOldObjects.end())
            unmatchedNewObjectIndices.append(i);
        else {
            bool textChanged = iterator->value.text != newObject.text;

            if (!newObject.descendants.isEmpty()) {
                // This is an atomic region, indicated by the presence of children.
                HashSet oldDescendantsCopy = iterator->value.descendants;
                HashSet newDescendantsCopy = newObject.descendants;

                newDescendantsCopy.removeAll(oldDescendantsCopy);
                oldDescendantsCopy.removeAll(newObject.descendants);
                bool nodesAdded = newDescendantsCopy.size();
                bool nodesRemoved = oldDescendantsCopy.size();

                if (nodesAdded && textChanged)
                    additions.append({ i, newObject });
                else if (nodesRemoved && textChanged)
                    diff.removed.append(newObject);

                if (textChanged)
                    diff.changed.append(newObject);
            } else if (textChanged)
                diff.changed.append(newObject);

            unmatchedOldObjects.remove(iterator);
        }
    }

    // Second pass. For new objects that had no AXID match, try to match them to a leftover old object with
    // identical text. Re-rendering a region destroys and recreates the objects holding text that was
    // already announced, so matching only by AXID makes that text look new and announces it again.
    for (auto index : unmatchedNewObjectIndices) {
        const auto& newObject = newObjects[index];

        bool matchedIdenticalText = false;
        // The text is only looked up when it is non-empty, matching how oldObjectIDsByText was built.
        if (!newObject.text.isEmpty()) {
            if (auto iterator = oldObjectIDsByText.find(newObject.text); iterator != oldObjectIDsByText.end()) {
                // An old object's text can only be claimed once, so drop candidates already matched by AXID above.
                while (!iterator->value.isEmpty()) {
                    if (unmatchedOldObjects.remove(iterator->value.takeLast())) {
                        matchedIdenticalText = true;
                        break;
                    }
                }
            }
        }

        // Text carried over unchanged from the previous snapshot has nothing to announce.
        if (!matchedIdenticalText)
            additions.append({ index, newObject });
    }

    // Merge the two passes back into document order. Announcing a new object before a changed atomic
    // region that precedes it would reverse the reading order of the announcement.
    std::sort(additions.begin(), additions.end(), [] (const auto& a, const auto& b) {
        return a.first < b.first;
    });
    diff.added.reserveInitialCapacity(additions.size());
    for (auto& addition : additions)
        diff.added.append(WTF::move(addition.second));

    // Anything left unmatched is a removal. Walk the old objects rather than the map so that removals
    // come out in document order rather than hash order.
    for (auto& oldObject : oldObjects) {
        if (unmatchedOldObjects.contains(oldObject.objectID))
            diff.removed.append({ oldObject.objectID, oldObject.text, oldObject.language, { } });
    }

    return diff;
}

static const size_t maximumAnnouncementLength = 2500;
enum class IsLiveRegionRemoval : bool { No, Yes };

AttributedString AXLiveRegionManager::computeAnnouncement(OptionSet<LiveRegionRelevant> liveRegionRelevant, const LiveRegionDiff& diff) const
{
    bool hasAll = liveRegionRelevant.contains(LiveRegionRelevant::All);
    bool hasAdditions = hasAll || liveRegionRelevant.contains(LiveRegionRelevant::Additions);
    bool hasRemovals = hasAll || liveRegionRelevant.contains(LiveRegionRelevant::Removals);
    bool hasText = hasAll || liveRegionRelevant.contains(LiveRegionRelevant::Text);

    StringBuilder stringBuilder;
    Vector<std::pair<AttributedString::Range, HashMap<String, AttributedString::AttributeValue>>> attributes;

    size_t characterCount = 0;

    HashSet<AXID> spokenObjects = { };

    // Determines whether we should add a space before adding the next object. Should only be false the first call.
    bool needsSpace = false;

    auto appendStringAndLanguage = [&](const LiveRegionObject& object, IsLiveRegionRemoval isRemoval = IsLiveRegionRemoval::No) {
        if (object.text.isEmpty() || spokenObjects.contains(object.objectID))
            return;

        if (needsSpace) {
            stringBuilder.append(' ');
            characterCount++;
        }

        uint64_t startLocation = stringBuilder.length();
        stringBuilder.append(object.text);
        characterCount += object.text.length();

        if (!object.language.isEmpty()) {
            HashMap<String, AttributedString::AttributeValue> languageAttribute;
            languageAttribute.set(accessibilityLanguageAttributeKey, AttributedString::AttributeValue { object.language });
            // The - / + 1 allows us to set the language of the space character seemlessly with the text around it.
            attributes.append({ { needsSpace && startLocation ? startLocation - 1 : startLocation, needsSpace && startLocation ? object.text.length() + 1 : object.text.length() }, WTF::move(languageAttribute) });
        }

        if (isRemoval == IsLiveRegionRemoval::Yes) {
            HashMap<String, AttributedString::AttributeValue> removalAttribute;
            removalAttribute.set(accessibilityIsLiveRegionRemovalAttributeKey, AttributedString::AttributeValue { 1.0 });
            attributes.append({ { needsSpace && startLocation ? startLocation - 1 : startLocation, needsSpace && startLocation ? object.text.length() + 1 : object.text.length() }, WTF::move(removalAttribute) });
        }

        // If the preceeding object already ends with a space (e.g., list markers), no need to add another.
        needsSpace = object.text.isEmpty() || object.text[object.text.length() - 1] != ' ';
        spokenObjects.add(object.objectID);
    };

    auto announceObjects = [&](const Vector<LiveRegionObject>& objects, IsLiveRegionRemoval isRemoval = IsLiveRegionRemoval::No, bool textContentOnly = false) {
        for (auto& object : objects) {
            if (characterCount > maximumAnnouncementLength)
                break;
            if (textContentOnly && !object.isTextContent)
                continue;
            appendStringAndLanguage(object, isRemoval);
        }
    };

    // "additions" announces all new nodes. "text" without "additions" announces only added text
    // nodes (e.g. textContent/innerText replacement creates new text nodes, not element additions).
    // When both are set, "additions" already covers text nodes so the text-only path is skipped.
    if (hasAdditions)
        announceObjects(diff.added);
    else if (hasText)
        announceObjects(diff.added, IsLiveRegionRemoval::No, /* textContentOnly */ true);

    if (hasRemovals)
        announceObjects(diff.removed, IsLiveRegionRemoval::Yes);

    if (hasText)
        announceObjects(diff.changed);

    auto string = stringBuilder.toString();
    return AttributedString { WTF::move(string), WTF::move(attributes), std::nullopt };
}

void AXLiveRegionManager::postAnnouncementForChange(AccessibilityObject& object, const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot)
{
    auto diff = computeChanges(oldSnapshot, newSnapshot);
    if (diff.added.isEmpty() && diff.removed.isEmpty() && diff.changed.isEmpty())
        return;

    // Collect the text of every object that could contribute to the announcement, in the order
    // computeAnnouncement() will consider them, so translated text can be written back positionally.
    Vector<String> segments;
    auto collect = [&](const Vector<LiveRegionObject>& objects) {
        for (auto& liveRegionObject : objects)
            segments.append(liveRegionObject.text);
    };
    collect(diff.added);
    collect(diff.removed);
    collect(diff.changed);

    // Translation has to happen before computeAnnouncement(), not after, because it concatenates these
    // objects into one AttributedString with per-range language and removal attributes, and word
    // order changes across languages would make those ranges unrecoverable. Assembling afterwards
    // also applies maximumAnnouncementLength to the translated text, which matters because
    // translations commonly run longer than their source.
    auto expectedSegmentCount = segments.size();
    // Capture only the two values the announcement needs. Capturing the snapshot would deep copy every
    // object in the region on every update just to read them.
    auto liveRegionStatus = newSnapshot.liveRegionStatus;
    auto liveRegionRelevant = newSnapshot.liveRegionRelevant;
    auto assemble = [protectedThis = CheckedRef { *this }, object = Ref { object }, diff, liveRegionStatus, liveRegionRelevant, expectedSegmentCount](Vector<String>&& translatedSegments, const String& language) mutable {
        if (translatedSegments.size() == expectedSegmentCount) {
            size_t index = 0;
            auto applyTranslation = [&](Vector<LiveRegionObject>& objects) {
                for (auto& liveRegionObject : objects) {
                    liveRegionObject.text = WTF::move(translatedSegments[index++]);
                    if (!language.isEmpty())
                        liveRegionObject.language = language;
                }
            };
            applyTranslation(diff.added);
            applyTranslation(diff.removed);
            applyTranslation(diff.changed);
        }

        AttributedString announcement = protectedThis->computeAnnouncement(liveRegionRelevant, diff);
        if (announcement.isNull() || announcement.string.isEmpty())
            return;

        CheckedRef { protectedThis->m_cache }->postLiveRegionNotification(object, liveRegionStatus, announcement);
    };

    CheckedRef { m_cache }->translateAnnouncementThenAssemble(Ref { object }, WTF::move(segments), WTF::move(assemble));
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

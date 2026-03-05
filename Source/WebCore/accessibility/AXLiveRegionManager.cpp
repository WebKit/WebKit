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
    String lowercaseString = string.convertToASCIILowercase();
    if (lowercaseString == "assertive")
        return LiveRegionStatus::Assertive;
    if (lowercaseString == "polite")
        return LiveRegionStatus::Polite;

    return LiveRegionStatus::Off;
}

static OptionSet<LiveRegionRelevant> stringToLiveRegionRelevant(const String& string)
{
    Vector<String> strings = string.convertToASCIILowercase().split(" "_s);
    OptionSet<LiveRegionRelevant> result;
    for (const auto& attribute : strings) {
        if (attribute == "additions")
            result.add(LiveRegionRelevant::Additions);
        else if (attribute == "all")
            result.add(LiveRegionRelevant::All);
        else if (attribute == "removals")
            result.add(LiveRegionRelevant::Removals);
        else if (attribute == "text")
            result.add(LiveRegionRelevant::Text);
    }
    return result;
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
    LiveRegionSnapshot newSnapshot = buildLiveRegionSnapshot(object);

    iterator->value = newSnapshot;

    postAnnouncementForChange(object, oldSnapshot, newSnapshot);
}

LiveRegionSnapshot AXLiveRegionManager::buildLiveRegionSnapshot(AccessibilityObject& object) const
{
    LiveRegionSnapshot snapshot;
    snapshot.liveRegionStatus = stringToLiveRegionStatus(object.liveRegionStatus());
    snapshot.liveRegionRelevant = stringToLiveRegionRelevant(object.liveRegionRelevant());

    std::function<void(AccessibilityObject&)> buildObjectList = [this, &buildObjectList, &snapshot] (AccessibilityObject& object) {
        // Treat atomic objects as one object, so when they change the entire subtree is announced.
        if (object.liveRegionAtomic()) {
            HashSet<AXID> descendants;

            // Collect all atomic-region descendants to detect when nodes are added/removed within the atomic region.
            std::function<void(AccessibilityObject&)> collectDescendants = [&collectDescendants, &descendants] (AccessibilityObject& descendant) {
                descendants.add(descendant.objectID());
                for (auto& child : descendant.unignoredChildren())
                    collectDescendants(downcast<AccessibilityObject>(child.get()));
            };

            for (auto& child : object.unignoredChildren())
                collectDescendants(downcast<AccessibilityObject>(child.get()));

            snapshot.objects.append({ object.objectID(), object.announcementText(), object.languageIncludingAncestors(), WTF::move(descendants) });
            return;
        }

        if (shouldIncludeInSnapshot(object))
            snapshot.objects.append({ object.objectID(), object.announcementText(), object.languageIncludingAncestors(), { } });
        else {
            for (auto& child : object.unignoredChildren())
                buildObjectList(downcast<AccessibilityObject>(child.get()));
        }
    };

    buildObjectList(object);

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

AXLiveRegionManager::LiveRegionDiff AXLiveRegionManager::computeChanges(const Vector<LiveRegionObject>& oldObjects, const Vector<LiveRegionObject>& newObjects) const
{
    // Here we compare the old and new live region to compute:
    // - Additions: New objects, or atomic regions where nodes were added AND text changed.
    // - Deletions: Objects that were removed from the region, or atomic regions where nodes were removed AND text changed.
    // - Changes: Text content/values that changed between the same object (without node additions/removals).

    LiveRegionDiff diff;

    // Build a map of old objects for lookup. As we match them with new objects, we'll remove them.
    // Whatever remains unmatched at the end represents removals.
    HashMap<AXID, LiveRegionObjectMetadata> unmatchedOldObjects;
    unmatchedOldObjects.reserveInitialCapacity(oldObjects.size());

    for (auto& object : oldObjects)
        unmatchedOldObjects.set(object.objectID, LiveRegionObjectMetadata { object.text, object.language, object.descendants });

    for (auto& newObject : newObjects) {
        auto iterator = unmatchedOldObjects.find(newObject.objectID);
        if (iterator == unmatchedOldObjects.end())
            diff.added.append(newObject);
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
                    diff.added.append(newObject);
                else if (nodesRemoved && textChanged)
                    diff.removed.append(newObject);

                if (textChanged)
                    diff.changed.append(newObject);
            } else if (textChanged)
                diff.changed.append(newObject);

            unmatchedOldObjects.remove(iterator);
        }
    }

    // Anything left in unmatchedOldObjects is a removal.
    for (auto& entry : unmatchedOldObjects)
        diff.removed.append({ entry.key, entry.value.text, entry.value.language, { } });

    return diff;
}

static const size_t maximumAnnouncementLength = 2500;
enum class IsLiveRegionRemoval : bool { No, Yes };

AttributedString AXLiveRegionManager::computeAnnouncement(const LiveRegionSnapshot& newSnapshot, const LiveRegionDiff& diff) const
{
    bool hasAll = newSnapshot.liveRegionRelevant.contains(LiveRegionRelevant::All);
    bool hasAdditions = hasAll || newSnapshot.liveRegionRelevant.contains(LiveRegionRelevant::Additions);
    bool hasRemovals = hasAll || newSnapshot.liveRegionRelevant.contains(LiveRegionRelevant::Removals);
    bool hasText = hasAll || newSnapshot.liveRegionRelevant.contains(LiveRegionRelevant::Text);

    StringBuilder stringBuilder;
    Vector<std::pair<AttributedString::Range, HashMap<String, AttributedString::AttributeValue>>> attributes;

    bool reachedCharacterLimit = false;
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

    if (hasAdditions && !diff.added.isEmpty()) {
        for (auto& object : diff.added) {
            appendStringAndLanguage(object);

            if (characterCount > maximumAnnouncementLength) {
                reachedCharacterLimit = true;
                break;
            }
        }
    }

    if (!reachedCharacterLimit && hasRemovals && !diff.removed.isEmpty()) {
        for (auto& object : diff.removed) {
            appendStringAndLanguage(object, IsLiveRegionRemoval::Yes);

            if (characterCount > maximumAnnouncementLength) {
                reachedCharacterLimit = true;
                break;
            }
        }
    }

    if (!reachedCharacterLimit && hasText && !diff.changed.isEmpty()) {
        for (auto& object : diff.changed) {
            appendStringAndLanguage(object);

            if (characterCount > maximumAnnouncementLength) {
                reachedCharacterLimit = true;
                break;
            }
        }
    }

    auto string = stringBuilder.toString();
    return AttributedString { WTF::move(string), WTF::move(attributes), std::nullopt };
}

void AXLiveRegionManager::postAnnouncementForChange(AccessibilityObject& object, const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot)
{
    auto diff = computeChanges(oldSnapshot.objects, newSnapshot.objects);
    if (diff.added.isEmpty() && diff.removed.isEmpty() && diff.changed.isEmpty())
        return;

    AttributedString announcement = computeAnnouncement(newSnapshot, diff);
    if (announcement.isNull() || announcement.string.isEmpty())
        return;

    if (CheckedPtr cache = m_cache)
        cache->postLiveRegionNotification(object, newSnapshot.liveRegionStatus, announcement);
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

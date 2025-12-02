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
#include "LocalizedStrings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AXLiveRegionManager);

#if PLATFORM(MAC)
static constexpr ASCIILiteral accessibilityLanguageAttributeKey = "AXLanguage"_s;
#else
static constexpr ASCIILiteral accessibilityLanguageAttributeKey = "UIAccessibilitySpeechAttributeLanguage"_s;
#endif

struct StringLanguagePair {
    String text;
    String language;
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
            snapshot.objects.append({ object.objectID(), textForObject(object), object.languageIncludingAncestors() });
            return;
        }

        if (shouldIncludeInSnapshot(object))
            snapshot.objects.append({ object.objectID(), textForObject(object), object.languageIncludingAncestors() });

        for (auto& child : object.unignoredChildren())
            buildObjectList(downcast<AccessibilityObject>(child.get()));
    };

    buildObjectList(object);

    return snapshot;
}

bool AXLiveRegionManager::shouldIncludeInSnapshot(AccessibilityObject& object) const
{
    if (object.isStaticText())
        return true;

    // If an object has unignored children, there isn't a need to include it in the snapshot since the children will return YES.
    if (object.firstUnignoredChild())
        return false;

    // For leaf objects, include if they have a value (e.g., form controls).
    if (!object.stringValue().isEmpty())
        return true;

#if PLATFORM(COCOA)
    // For leaf objects, include if they have accessible description text (e.g., images with alt text).
    if (!object.descriptionAttributeValue().isEmpty())
        return true;
#endif

    return false;
}

String AXLiveRegionManager::textForObject(AccessibilityObject& object) const
{
    return object.textMarkerRange().toString(IncludeListMarkerText::Yes, IncludeImageAltText::Yes);
}

AXLiveRegionManager::LiveRegionDiff AXLiveRegionManager::computeChanges(const Vector<LiveRegionObject>& oldObjects, const Vector<LiveRegionObject>& newObjects) const
{
    // Here we compare the old and new live region to compute:
    // - Additions: New objects
    // - Deletions: Objects that were removed from the region
    // - Changes: Text content/values that changed between the same object.

    LiveRegionDiff diff;

    // Build a map of old objects for lookup. As we match them with new objects, we'll remove them.
    // Whatever remains unmatched at the end represents removals.
    HashMap<AXID, StringLanguagePair> unmatchedOldObjects;
    unmatchedOldObjects.reserveInitialCapacity(oldObjects.size());

    for (auto& object : oldObjects)
        unmatchedOldObjects.set(object.objectID, StringLanguagePair { object.text, object.language });

    for (auto& newObject : newObjects) {
        auto iterator = unmatchedOldObjects.find(newObject.objectID);
        if (iterator == unmatchedOldObjects.end())
            diff.added.append(newObject);
        else {
            if (iterator->value.text != newObject.text)
                diff.changed.append(newObject);
            unmatchedOldObjects.remove(iterator);
        }
    }

    // Anything left in unmatchedOldObjects is a removal.
    for (auto& entry : unmatchedOldObjects)
        diff.removed.append({ entry.key, entry.value.text, entry.value.language });

    return diff;
}

static const size_t maximumAnnouncementLength = 2500;

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

    // Determines whether we should add a space before adding the next object. Should only be false the first call.
    bool needsSpace = false;

    auto appendStringAndLanguage = [&](const LiveRegionObject& object) {
        if (object.text.isEmpty())
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
            attributes.append({ { startLocation, object.text.length() }, WTFMove(languageAttribute) });
        }

        needsSpace = true;
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
        if (needsSpace) {
            stringBuilder.append(' ');
            characterCount++;
        }

        String removalPrefix = AXRemovedText();
        characterCount += removalPrefix.length();
        stringBuilder.append(WTFMove(removalPrefix));
        needsSpace = true;

        for (auto& object : diff.removed) {
            appendStringAndLanguage(object);

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
    return AttributedString { WTFMove(string), WTFMove(attributes), std::nullopt };
}

void AXLiveRegionManager::postAnnouncementForChange(AccessibilityObject& object, const LiveRegionSnapshot& oldSnapshot, const LiveRegionSnapshot& newSnapshot)
{
    auto diff = computeChanges(oldSnapshot.objects, newSnapshot.objects);
    if (diff.added.isEmpty() && diff.removed.isEmpty() && diff.changed.isEmpty())
        return;

    AttributedString announcement = computeAnnouncement(newSnapshot, diff);
    if (announcement.isNull())
        return;

    if (CheckedPtr cache = m_cache)
        cache->postLiveRegionNotification(object, newSnapshot.liveRegionStatus, announcement);
}

} // namespace WebCore

#endif // PLATFORM(COCOA)

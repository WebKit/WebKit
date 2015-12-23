/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM)

#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSource.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "UUID.h"
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

using namespace WebCore;

CaptureDeviceManager::~CaptureDeviceManager()
{
}

Vector<RefPtr<TrackSourceInfo>> CaptureDeviceManager::getSourcesInfo(const String& requestOrigin)
{
    UNUSED_PARAM(requestOrigin);
    Vector<RefPtr<TrackSourceInfo>> sourcesInfo;
    for (auto captureDevice : captureDeviceList()) {
        if (!captureDevice.m_enabled || captureDevice.m_sourceType == RealtimeMediaSource::None)
            continue;

        TrackSourceInfo::SourceKind trackSourceType = captureDevice.m_sourceType == RealtimeMediaSource::Video ? TrackSourceInfo::Video : TrackSourceInfo::Audio;
        sourcesInfo.append(TrackSourceInfo::create(captureDevice.m_persistentDeviceID, captureDevice.m_sourceId, trackSourceType, captureDevice.m_localizedName, captureDevice.m_groupID));
    }
    LOG(Media, "CaptureDeviceManager::getSourcesInfo(%p), found %zu active devices", this, sourcesInfo.size());
    return sourcesInfo;
}

bool CaptureDeviceManager::captureDeviceFromDeviceID(const String& captureDeviceID, CaptureDeviceInfo& foundDevice)
{
    for (auto& device : captureDeviceList()) {
        if (device.m_persistentDeviceID == captureDeviceID) {
            foundDevice = device;
            return true;
        }
    }

    return false;
}

bool CaptureDeviceManager::verifyConstraintsForMediaType(RealtimeMediaSource::Type type, MediaConstraints* constraints, const CaptureSessionInfo* session, String& invalidConstraint)
{
    if (!constraints)
        return true;

    Vector<MediaConstraint> mandatoryConstraints;
    constraints->getMandatoryConstraints(mandatoryConstraints);
    for (auto& constraint : mandatoryConstraints) {
        if (sessionSupportsConstraint(session, type, constraint.m_name, constraint.m_value))
            continue;

        invalidConstraint = constraint.m_name;
        return false;
    }

    return true;
}

Vector<RefPtr<RealtimeMediaSource>> CaptureDeviceManager::bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type type, PassRefPtr<MediaConstraints> constraints)
{
    Vector<RefPtr<RealtimeMediaSource>> bestSourcesList;

    struct {
        bool operator()(RefPtr<RealtimeMediaSource> a, RefPtr<RealtimeMediaSource> b)
        {
            return a->fitnessScore() < b->fitnessScore();
        }
    } sortBasedOnFitnessScore;

    for (auto& captureDevice : captureDeviceList()) {
        if (!captureDevice.m_enabled || captureDevice.m_sourceId.isEmpty() || captureDevice.m_sourceType == RealtimeMediaSource::None)
            continue;

        if (RefPtr<RealtimeMediaSource> captureSource = sourceWithUID(captureDevice.m_persistentDeviceID, type, constraints.get()))
            bestSourcesList.append(captureSource.leakRef());
    }
    std::sort(bestSourcesList.begin(), bestSourcesList.end(), sortBasedOnFitnessScore);
    return bestSourcesList;
}

RefPtr<RealtimeMediaSource> CaptureDeviceManager::sourceWithUID(const String& deviceUID, RealtimeMediaSource::Type type, MediaConstraints* constraints)
{
    for (auto& captureDevice : captureDeviceList()) {
        if (captureDevice.m_persistentDeviceID != deviceUID || captureDevice.m_sourceType != type)
            continue;

        if (!captureDevice.m_enabled || type == RealtimeMediaSource::None || captureDevice.m_sourceId.isEmpty())
            continue;

        if (RealtimeMediaSource* mediaSource = createMediaSourceForCaptureDeviceWithConstraints(captureDevice, constraints))
            return mediaSource;
    }
    return nullptr;
}

CaptureDeviceInfo* CaptureDeviceManager::bestDeviceForFacingMode(RealtimeMediaSourceSettings::VideoFacingMode facingMode)
{
    if (facingMode == RealtimeMediaSourceSettings::Unknown)
        return nullptr;

    for (auto& device : captureDeviceList()) {
        if (device.m_sourceType == RealtimeMediaSource::Video && device.m_position == facingMode)
            return &device;
    }
    return nullptr;
}

static inline RealtimeMediaSourceSettings::VideoFacingMode facingModeFromString(const String& facingModeString)
{
    static NeverDestroyed<AtomicString> userFacingModeString("user", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> environmentFacingModeString("environment", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> leftFacingModeString("left", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> rightFacingModeString("right", AtomicString::ConstructFromLiteral);
    if (facingModeString == userFacingModeString)
        return RealtimeMediaSourceSettings::User;
    if (facingModeString == environmentFacingModeString)
        return RealtimeMediaSourceSettings::Environment;
    if (facingModeString == leftFacingModeString)
        return RealtimeMediaSourceSettings::Left;
    if (facingModeString == rightFacingModeString)
        return RealtimeMediaSourceSettings::Right;
    return RealtimeMediaSourceSettings::Unknown;
}

bool CaptureDeviceManager::sessionSupportsConstraint(const CaptureSessionInfo*, RealtimeMediaSource::Type type, const String& name, const String& value)
{
    const RealtimeMediaSourceSupportedConstraints& supportedConstraints = RealtimeMediaSourceCenter::singleton().supportedConstraints();
    MediaConstraintType constraint = supportedConstraints.constraintFromName(name);
    if (!supportedConstraints.supportsConstraint(constraint))
        return false;

    switch (constraint) {
    case MediaConstraintType::Width:
        return type == RealtimeMediaSource::Video;

    case MediaConstraintType::Height:
        return type == RealtimeMediaSource::Video;

    case MediaConstraintType::FrameRate: {
        if (type == RealtimeMediaSource::Audio)
            return false;

        return isSupportedFrameRate(value.toFloat());
    }
    case MediaConstraintType::FacingMode: {
        if (type == RealtimeMediaSource::Audio)
            return false;

        return bestDeviceForFacingMode(facingModeFromString(value));
    }
    default:
        return false;
    }
}

bool CaptureDeviceManager::isSupportedFrameRate(float frameRate) const
{
    return 0 < frameRate && frameRate <= 60;
}

#endif // ENABLE(MEDIA_STREAM)

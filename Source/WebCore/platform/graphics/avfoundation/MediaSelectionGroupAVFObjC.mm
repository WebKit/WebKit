/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaSelectionGroupAVFObjC.h"

#if ENABLE(VIDEO_TRACK)

#import "SoftLinking.h"
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVPlayerItem.h>
#import <objc/runtime.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionGroup)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionOption)

namespace WebCore {

PassRefPtr<MediaSelectionOptionAVFObjC> MediaSelectionOptionAVFObjC::create(MediaSelectionGroupAVFObjC& group, AVMediaSelectionOption *option)
{
    return adoptRef(new MediaSelectionOptionAVFObjC(group, option));
}

MediaSelectionOptionAVFObjC::MediaSelectionOptionAVFObjC(MediaSelectionGroupAVFObjC& group, AVMediaSelectionOption *option)
    : m_group(&group)
    , m_mediaSelectionOption(option)
{
}

void MediaSelectionOptionAVFObjC::setSelected(bool selected)
{
    if (!m_group)
        return;

    if (selected == this->selected())
        return;

    m_group->setSelectedOption(selected ? this : nullptr);
}

bool MediaSelectionOptionAVFObjC::selected() const
{
    if (!m_group)
        return false;
    return this == m_group->selectedOption();
}

int MediaSelectionOptionAVFObjC::index() const
{
    if (!m_group)
        return 0;

    return [[m_group->avMediaSelectionGroup() options] indexOfObject:m_mediaSelectionOption.get()];
}

PassRefPtr<MediaSelectionGroupAVFObjC> MediaSelectionGroupAVFObjC::create(AVPlayerItem *item, AVMediaSelectionGroup *group)
{
    return adoptRef(new MediaSelectionGroupAVFObjC(item, group));
}

MediaSelectionGroupAVFObjC::MediaSelectionGroupAVFObjC(AVPlayerItem *item, AVMediaSelectionGroup *group)
    : m_playerItem(item)
    , m_mediaSelectionGroup(group)
    , m_selectedOption(nullptr)
    , m_selectionTimer(this, &MediaSelectionGroupAVFObjC::selectionTimerFired)
{
    updateOptions();
}

MediaSelectionGroupAVFObjC::~MediaSelectionGroupAVFObjC()
{
    for (auto& option : m_options.values())
        option->clearGroup();
}

void MediaSelectionGroupAVFObjC::updateOptions()
{
    RetainPtr<NSSet> newAVOptions = adoptNS([[NSSet alloc] initWithArray:[getAVMediaSelectionGroupClass() playableMediaSelectionOptionsFromArray:[m_mediaSelectionGroup options]]]);
    RetainPtr<NSMutableSet> oldAVOptions = adoptNS([[NSMutableSet alloc] initWithCapacity:m_options.size()]);
    for (auto& avOption : m_options.keys())
        [oldAVOptions addObject:avOption];

    RetainPtr<NSMutableSet> addedAVOptions = [newAVOptions mutableCopy];
    [addedAVOptions minusSet:oldAVOptions.get()];

    RetainPtr<NSMutableSet> removedAVOptions = [oldAVOptions mutableCopy];
    [removedAVOptions minusSet:newAVOptions.get()];

    for (AVMediaSelectionOption* removedAVOption in removedAVOptions.get()) {
        if (m_selectedOption && removedAVOption == m_selectedOption->avMediaSelectionOption())
            m_selectedOption = nullptr;

        m_options.remove(removedAVOption);
    }

    AVMediaSelectionOption* selectedOption = [m_playerItem selectedMediaOptionInMediaSelectionGroup:m_mediaSelectionGroup.get()];

    for (AVMediaSelectionOption* addedAVOption in addedAVOptions.get()) {
        RefPtr<MediaSelectionOptionAVFObjC> addedOption = MediaSelectionOptionAVFObjC::create(*this, addedAVOption);
        if (addedAVOption == selectedOption)
            m_selectedOption = addedOption.get();
        m_options.set(addedAVOption, addedOption.release());
    }
}

void MediaSelectionGroupAVFObjC::setSelectedOption(MediaSelectionOptionAVFObjC* option)
{
    if (m_selectedOption == option)
        return;

    m_selectedOption = option;
    if (m_selectionTimer.isActive())
        m_selectionTimer.stop();
    m_selectionTimer.startOneShot(0);
}

void MediaSelectionGroupAVFObjC::selectionTimerFired(Timer&)
{
    [m_playerItem selectMediaOption:(m_selectedOption ? m_selectedOption->avMediaSelectionOption() : nil) inMediaSelectionGroup:m_mediaSelectionGroup.get()];
}

}

#endif

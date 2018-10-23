/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#pragma once

#include "FrameLoaderTypes.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Frame;
class HistoryItem;
class SerializedScriptValue;

enum class ShouldTreatAsContinuingLoad : bool;

struct StringWithDirection;

class HistoryController {
    WTF_MAKE_NONCOPYABLE(HistoryController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum HistoryUpdateType { UpdateAll, UpdateAllExceptBackForwardList };

    explicit HistoryController(Frame&);
    ~HistoryController();

    WEBCORE_EXPORT void saveScrollPositionAndViewStateToItem(HistoryItem*);
    void clearScrollPositionAndViewState();
    WEBCORE_EXPORT void restoreScrollPositionAndViewState();

    void updateBackForwardListForFragmentScroll();

    void saveDocumentState();
    WEBCORE_EXPORT void saveDocumentAndScrollState();
    void restoreDocumentState();

    void invalidateCurrentItemCachedPage();

    void updateForBackForwardNavigation();
    void updateForReload();
    void updateForStandardLoad(HistoryUpdateType updateType = UpdateAll);
    void updateForRedirectWithLockedBackForwardList();
    void updateForClientRedirect();
    void updateForCommit();
    void updateForSameDocumentNavigation();
    void updateForFrameLoadCompleted();

    HistoryItem* currentItem() const { return m_currentItem.get(); }
    WEBCORE_EXPORT void setCurrentItem(HistoryItem&);
    void setCurrentItemTitle(const StringWithDirection&);
    bool currentItemShouldBeReplaced() const;
    WEBCORE_EXPORT void replaceCurrentItem(HistoryItem*);

    HistoryItem* previousItem() const { return m_previousItem.get(); }
    void clearPreviousItem();

    HistoryItem* provisionalItem() const { return m_provisionalItem.get(); }
    void setProvisionalItem(HistoryItem*);

    void pushState(RefPtr<SerializedScriptValue>&&, const String& title, const String& url);
    void replaceState(RefPtr<SerializedScriptValue>&&, const String& title, const String& url);

    void setDefersLoading(bool);

private:
    friend class Page;
    bool shouldStopLoadingForHistoryItem(HistoryItem&) const;
    void goToItem(HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad);

    void initializeItem(HistoryItem&);
    Ref<HistoryItem> createItem();
    Ref<HistoryItem> createItemTree(Frame& targetFrame, bool clipAtTarget);

    void recursiveSetProvisionalItem(HistoryItem&, HistoryItem*);
    void recursiveGoToItem(HistoryItem&, HistoryItem*, FrameLoadType, ShouldTreatAsContinuingLoad);
    bool isReplaceLoadTypeWithProvisionalItem(FrameLoadType);
    bool isReloadTypeWithProvisionalItem(FrameLoadType);
    void recursiveUpdateForCommit();
    void recursiveUpdateForSameDocumentNavigation();
    bool itemsAreClones(HistoryItem&, HistoryItem*) const;
    bool currentFramesMatchItem(HistoryItem&) const;
    void updateBackForwardListClippedAtTarget(bool doClip);
    void updateCurrentItem();

    Frame& m_frame;

    RefPtr<HistoryItem> m_currentItem;
    RefPtr<HistoryItem> m_previousItem;
    RefPtr<HistoryItem> m_provisionalItem;

    bool m_frameLoadComplete;

    bool m_defersLoading;
    RefPtr<HistoryItem> m_deferredItem;
    FrameLoadType m_deferredFrameLoadType;
};

} // namespace WebCore

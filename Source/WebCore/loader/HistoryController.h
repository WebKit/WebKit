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

#include <WebCore/BackForwardItemIdentifier.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/ProcessSwapDisposition.h>

namespace WebCore {

class HistoryItem;
class HistoryItemClient;
class LocalFrame;
class SerializedScriptValue;

enum class ShouldGoToHistoryItem : uint8_t;
enum class ShouldTreatAsContinuingLoad : uint8_t;

struct NavigationAPIMethodTracker;
struct StringWithDirection;

class HistoryController final : public CanMakeWeakPtr<HistoryController>  {
    WTF_MAKE_NONCOPYABLE(HistoryController);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(HistoryController, Loader);
public:
    enum HistoryUpdateType { UpdateAll, UpdateAllExceptBackForwardList };

    explicit HistoryController(LocalFrame&);
    ~HistoryController();

    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;

    WEBCORE_EXPORT void saveScrollPositionAndViewStateToItem(HistoryItem*);
    WEBCORE_EXPORT void restoreScrollPositionAndViewState();

    void updateBackForwardListForFragmentScroll();
    void updateBackForwardListForReplaceState(RefPtr<SerializedScriptValue>&&, const String&);

    void saveDocumentState();
    WEBCORE_EXPORT void saveDocumentAndScrollState();
    void restoreDocumentState();

    void invalidateCurrentItemCachedPage();

    void updateForBackForwardNavigation();
    void updateForReloadOrReplace();
    void updateForStandardLoad(HistoryUpdateType updateType = UpdateAll);
    void updateForRedirectWithLockedBackForwardList();
    void updateForClientRedirect();
    void updateForCommit();
    void updateForSameDocumentNavigation();
    void updateForFrameLoadCompleted();

    HistoryItem* currentItem() const { return m_currentItem.get(); }
    WEBCORE_EXPORT void setCurrentItem(Ref<HistoryItem>&&);
    void setCurrentItemTitle(const StringWithDirection&);
    bool currentItemShouldBeReplaced() const;
    WEBCORE_EXPORT void replaceCurrentItem(RefPtr<HistoryItem>&&);

    HistoryItem* previousItem() const { return m_previousItem.get(); }
    void clearPreviousItem();

    HistoryItem* provisionalItem() const { return m_provisionalItem.get(); }
    void setProvisionalItem(RefPtr<HistoryItem>&&);

    void pushState(RefPtr<SerializedScriptValue>&&, const String& url);
    void replaceState(RefPtr<SerializedScriptValue>&&, const String& url);

    void setDefersLoading(bool);

    Ref<HistoryItem> createItemWithLoader(HistoryItemClient&, DocumentLoader*);

    WEBCORE_EXPORT RefPtr<HistoryItem> createItemTree(LocalFrame& targetFrame, bool clipAtTarget, BackForwardItemIdentifier);

    void clearPolicyItem();

private:
    friend class Page;
    bool shouldStopLoadingForHistoryItem(HistoryItem&) const;
    void goToItem(HistoryItem&, FrameLoadType, ShouldTreatAsContinuingLoad, ProcessSwapDisposition processSwapDisposition = ProcessSwapDisposition::None);
    void goToItemForNavigationAPI(HistoryItem&, FrameLoadType, LocalFrame& triggeringFrame, NavigationAPIMethodTracker*);
    void goToItemShared(HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&&, ProcessSwapDisposition processSwapDisposition = ProcessSwapDisposition::None);

    void initializeItem(HistoryItem&, RefPtr<DocumentLoader>);
    Ref<HistoryItem> createItem(HistoryItemClient&, BackForwardItemIdentifier);
    Ref<HistoryItem> createItemTree(HistoryItemClient&, LocalFrame& targetFrame, bool clipAtTarget, BackForwardItemIdentifier);

    enum class ForNavigationAPI : bool { No, Yes };
    void recursiveSetProvisionalItem(HistoryItem&, HistoryItem*, ForNavigationAPI = ForNavigationAPI::No);
    void recursiveGoToItem(HistoryItem&, HistoryItem*, FrameLoadType, ShouldTreatAsContinuingLoad);
    bool isMultipartReplaceLoadTypeWithProvisionalItem(FrameLoadType);
    bool isReloadTypeWithProvisionalItem(FrameLoadType);
    void recursiveUpdateForCommit();
    void recursiveUpdateForSameDocumentNavigation();
    static bool itemsAreClones(HistoryItem&, HistoryItem*);
    void updateBackForwardListClippedAtTarget(bool doClip);
    void updateCurrentItem();
    bool isFrameLoadComplete() const { return m_frameLoadComplete; }

    struct FrameToNavigate;
    static void recursiveGatherFramesToNavigate(LocalFrame&, Vector<FrameToNavigate>&, HistoryItem& targetItem, HistoryItem* fromItem);

    const WeakRef<LocalFrame> m_frame;

    RefPtr<HistoryItem> m_currentItem;
    RefPtr<HistoryItem> m_previousItem;
    RefPtr<HistoryItem> m_provisionalItem;
    RefPtr<HistoryItem> m_policyItem;

    bool m_frameLoadComplete;

    bool m_defersLoading;
    FrameLoadType m_deferredFrameLoadType;
    RefPtr<HistoryItem> m_deferredItem;
};

} // namespace WebCore

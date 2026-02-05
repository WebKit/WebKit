// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if compiler(>=6.2)

#if ENABLE_BACK_FORWARD_LIST_SWIFT

internal import WebCore_Private
internal import WebKit_Internal
internal import wtf

// A note on swift-format-ignore: NeverForceUnwrap:
// This file currently aims to closely adhere to the C++ original which uses
// RefPtr.get() and friends frequently; this is functionally similar to force
// unwrapping so that's been retained.

// rdar://164119356 may allow us to automate some of these conformances
// using SWIFT_CONFORMS_TO_PROTOCOL
extension WebKit.RefFrameState: CxxRef {
    typealias Pointee = WebKit.FrameState
}

extension WebKit.RefWebBackForwardListItem: CxxRef {
    typealias Pointee = WebKit.WebBackForwardListItem
}

extension WebKit.VectorRefFrameState: CxxRefVector {
    typealias Element = WebKit.RefFrameState
}

extension WebKit.BackForwardListItemVector: CxxRefVector {
    typealias Element = WebKit.RefWebBackForwardListItem
}

// Some of these utility functions would be better in WebBackForwardListSwiftUtilities.h
// but can't be put there as we are unable to use swift::Array and swift::String
// rdar://161270632

// Temporary partial WTF logging support from Swift
// rdar://168139823 is the task of exposing WebKit logging properly in Swift
private func backForwardLog(_ msgCreator: @autoclosure () -> String) {
    // rdar://133777029 likely will allow us to avoid the performance penalty
    // of creating the string if logging is disabled.
    let msg = msgCreator()

    let span = msg.utf8CString.span
    // Safety: the buffer pointer is guaranteed to be
    // valid and null-terminated during the call to
    // doLog
    unsafe span.withUnsafeBufferPointer { ptr in
        // swift-format-ignore: NeverForceUnwrap
        unsafe doLog(ptr.baseAddress!)
    }
}

private func loadingReleaseLog(_ msgCreator: @autoclosure () -> String) {
    let msg = msgCreator()

    let span = msg.utf8CString.span
    // Safety: the buffer pointer is guaranteed to be
    // valid and null-terminated during the call to
    // doLoadingReleaseLog
    unsafe span.withUnsafeBufferPointer { ptr in
        // swift-format-ignore: NeverForceUnwrap
        unsafe doLoadingReleaseLog(ptr.baseAddress!)
    }
}

// Temporary partial MESSAGE_CHECK_BASE support from Swift
// Idiomatic equivalent represented by rdar://168139740
private func messageCheck(process: WebKit.RefWebProcessProxy, _ assertion: @autoclosure () -> Bool) -> Bool {
    messageCheckCompletion(process: process, completionHandler: {}, assertion())
}

private func messageCheckCompletion(
    process: WebKit.RefWebProcessProxy,
    completionHandler: () -> Void,
    _ assertion: @autoclosure () -> Bool
) -> Bool {
    if !assertion() {
        messageCheckFailed(process)
        completionHandler()
        return true
    }
    return false
}

final class WebBackForwardList {
    private static let defaultCapacity = 100

    var page: WebKit.WeakPtrWebPageProxy
    // Optional just because of an initialization order issue.
    // Always occupied after initialization finished.
    var messageForwarder: RefWebBackForwardListMessageForwarder?

    var entries: [WebKit.WebBackForwardListItem] = []
    var currentIndex: Array.Index?

    private enum Direction {
        case backward
        case forward
    }

    init(page: WebKit.WeakPtrWebPageProxy) {
        self.page = page
        self.messageForwarder = WebKit.WebBackForwardListMessageForwarder.create(target: self)
    }

    func getMessageReceiver() -> RefWebBackForwardListMessageForwarder {
        // Guaranteed to be Some after construction
        // swift-format-ignore: NeverForceUnwrap
        self.messageForwarder!
    }

    func itemForID(identifier: WebCore.BackForwardItemIdentifier) -> WebKit.WebBackForwardListItem? {
    }

    func pageClosed() {
    }

    func addItem(newItem: WebKit.WebBackForwardListItem) {
    }

    func goToItem(item: WebKit.WebBackForwardListItem) {
    }

    func currentItem() -> WebKit.WebBackForwardListItem? {
    }

    func backItem() -> WebKit.WebBackForwardListItem? {
    }

    func forwardItem() -> WebKit.WebBackForwardListItem? {
    }

    func itemAtIndex(index: Array.Index) -> WebKit.WebBackForwardListItem? {
    }

    func backListCount() -> Array.Index {
    }

    func forwardListCount() -> Array.Index {
    }

    func backListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
    }

    func forwardListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
    }

    func removeAllItems() {
    }

    func clear() {
    }

    func backForwardListState(filter: WebBackForwardListItemFilter) -> WebKit.BackForwardListState {
    }

    func restoreFromState(backForwardListState: WebKit.BackForwardListState) {
    }

    func setItemsAsRestoredFromSession() {
    }

    func setItemsAsRestoredFromSessionIf(functor: WebBackForwardListItemFilter) {
    }

    func didRemoveItem(item: WebKit.WebBackForwardListItem) {
    }

    func goBackItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
    }

    func goForwardItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
    }

    func loggingString() -> Swift.String {
    }

    func setBackForwardItemIdentifier(frameState: WebKit.FrameState, itemID: WebCore.BackForwardItemIdentifier) {
    }

    func completeFrameStateForNavigation(navigatedFrameState: WebKit.FrameState) -> WebKit.FrameState {
    }

    func backForwardAddItemShared(
        connection: IPC.Connection,
        navigatedFrameState: WebKit.RefFrameState,
        loadedWebArchive: WebKit.LoadedWebArchive
    ) {
    }

    // IPCs from here on

    func backForwardAddItem(connection: IPC.Connection, navigatedFrameState: WebKit.RefFrameState) {
        if let page = page.get() {
            backForwardAddItemShared(
                connection: connection,
                navigatedFrameState: navigatedFrameState,
                loadedWebArchive: page.didLoadWebArchive() ? .Yes : .No
            )
        }
    }

    func backForwardSetChildItem(frameItemID: WebCore.BackForwardFrameItemIdentifier, frameState: WebKit.RefFrameState) {
        guard let item = currentItem() else {
            return
        }

        if let frameItem = WebKit.WebBackForwardListFrameItem.itemForID(item.identifier(), frameItemID) {
            frameItem.setChild(consuming: frameState)
        }
    }

    func backForwardClearChildren(itemID: WebCore.BackForwardItemIdentifier, frameItemID: WebCore.BackForwardFrameItemIdentifier) {
        if let frameItem = WebKit.WebBackForwardListFrameItem.itemForID(itemID, frameItemID) {
            frameItem.clearChildren()
        }
    }

    func backForwardUpdateItem(connection: IPC.Connection, frameState: WebKit.RefFrameState) {
        // __convertToBool necessary due to rdar://137879510
        if !frameState.ptr().itemID.__convertToBool() || !frameState.ptr().frameItemID.__convertToBool() {
            return
        }
        let itemID = frameState.ptr().itemID.pointee
        let frameItemID = frameState.ptr().frameItemID.pointee
        guard let frameItem = WebKit.WebBackForwardListFrameItem.itemForID(itemID, frameItemID) else {
            return
        }
        guard let item = frameItem.backForwardListItem() else {
            return
        }
        guard let webPageProxy = page.get() else {
            return
        }
        // We can't use == here due to rdar://162357139
        assert(contentsMatch(webPageProxy.identifier(), item.pageID()) && contentsMatch(itemID, item.identifier()))
        if let process = WebKit.AuxiliaryProcessProxy.fromConnection(connection) {
            // The downcast in C++ is really just used to assert that the process is a WebProcessProxy
            assert(downcastToWebProcessProxy(process).__convertToBool())
            let hasBackForwardCacheEntry = item.backForwardCacheEntry() != nil
            if hasBackForwardCacheEntry != frameState.ptr().hasCachedPage {
                // Safety: accessing suspendedPage pointer just to check nullness, no dereference occurs
                if frameState.ptr().hasCachedPage {
                    webPageProxy.backForwardCache().addEntry(item, process.coreProcessIdentifier())
                } else if unsafe item.suspendedPage() == nil {
                    webPageProxy.backForwardCache().removeEntry(item)
                }
            }
        }
        frameItem.setFrameState(consuming: frameState)
    }

    func backForwardGoToItem(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardGoToItemCompletionHandler
    ) {
        // On process swap, we tell the previous process to ignore the load, which causes it to restore its current back forward item to its previous
        // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
        // Any real new load in the committed process would have cleared m_provisionalPage.
        if let webPageProxy = page.get(), webPageProxy.hasProvisionalPage() {
            completionHandler.pointee(consuming: counts())
            return
        }

        backForwardGoToItemShared(itemID: itemID, completionHandler: completionHandler)
    }

    func backForwardListContainsItem(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardListContainsItemCompletionHandler
    ) {
        completionHandler.pointee(itemForID(identifier: itemID) != nil)
    }

    func backForwardGoToItemShared(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardGoToItemCompletionHandler
    ) {
        if let webPageProxy = page.get() {
            if messageCheckCompletion(
                process: WebKit.RefWebProcessProxy(webPageProxy.legacyMainFrameProcess()),
                assertion: { !WebKit.isInspectorPage(webPageProxy) },
                completionHandler: { completionHandler.pointee(consuming: counts()) }
            ) {
                return
            }
        }

        if let item = itemForID(identifier: itemID) {
            goToItem(item: item)
        }

        completionHandler.pointee(consuming: counts())
    }

    func backForwardAllItems(
        frameID: WebCore.FrameIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardAllItemsCompletionHandler
    ) {
        var frameStates: [WebKit.FrameState] = []
        for item in entries {
            if let frameItem = item.mainFrameItem().childItemForFrameID(frameID) {
                frameStates.append(frameItem.copyFrameStateWithChildren().ptr())
            }
        }
        completionHandler.pointee(consuming: WebKit.VectorRefFrameState(array: frameStates))
    }

    func backForwardItemAtIndex(
        index: Int32,
        frameID: WebCore.FrameIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardItemAtIndexCompletionHandler
    ) {
        // FIXME: This should verify that the web process requesting the item hosts the specified frame.
        let index = Int(index)
        guard let item = itemAtIndex(index: index) else {
            // Safety: believed to be a false positive, rdar://162608225
            unsafe completionHandler.pointee(consuming: WebKit.RefPtrFrameState())
            return
        }
        guard let frameItem = item.mainFrameItem().childItemForFrameID(frameID) else {
            // Safety: believed to be a false positive, rdar://162608225
            unsafe completionHandler.pointee(consuming: WebKit.RefPtrFrameState(item.mainFrameState().ptr()))
            return
        }
        // Safety: believed to be a false positive, rdar://162608225
        unsafe completionHandler.pointee(consuming: WebKit.RefPtrFrameState(frameItem.copyFrameStateWithChildren().ptr()))
    }

    func backForwardListCounts(completionHandler: CompletionHandlers.WebBackForwardList.BackForwardListCountsCompletionHandler) {
        completionHandler.pointee(consuming: counts())
    }
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

#endif // compiler(>=6.2)

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

#if compiler(>=6.2.3)

#if ENABLE_BACK_FORWARD_LIST_SWIFT

import WebCore_Private
import WebKit_Internal
import wtf

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

extension WebKit.VectorBackForwardListItemState: CxxVector {
    typealias Element = WebKit.BackForwardListItemState
}

extension WebKit.WebBackForwardListItem {
    private borrowing func getUrlCopy() -> WTF.String {
        // Safety: we immediately make a copy of the string before
        // it could be freed or mutated. FIXME(rdar://145054011): remove
        // this.
        unsafe __urlUnsafe().pointee
    }

    var url: WTF.String {
        getUrlCopy()
    }
}

// Some of these utility functions would be better in WebBackForwardListSwiftUtilities.h
// but can't be put there as we are unable to use swift::Array and swift::String
// rdar://161270632

// Temporary partial WTF logging support from Swift
// rdar://168139823 is the task of exposing WebKit logging properly in Swift
private func backForwardLog(_ msgCreator: @autoclosure () -> String) {
    // rdar://133777029 likely will allow us to avoid the performance penalty
    // of creating the string if logging is disabled.
    doLog(WTF.String(msgCreator()))
}

private func loadingReleaseLog(_ msgCreator: @autoclosure () -> String) {
    doLoadingReleaseLog(WTF.String(msgCreator()))
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

// FIXME(rdar://130765784): We should be able use the built-in ===, but AnyObject currently excludes foreign reference types
@_expose(!Cxx) // rdar://169474185
func === (_ lhs: WebKit.WebBackForwardListItem, _ rhs: WebKit.WebBackForwardListItem) -> Bool {
    // Safety: Swift represents all reference types, including foreign reference types, as raw pointers
    unsafe unsafeBitCast(lhs, to: UnsafeRawPointer.self) == unsafeBitCast(rhs, to: UnsafeRawPointer.self)
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
        backForwardLog("(Back/Forward) Created WebBackForwardList \(ObjectIdentifier(self))")
    }

    deinit {
        backForwardLog("(Back/Forward) Destroying WebBackForwardList \(ObjectIdentifier(self))")
        // A WebBackForwardList should never be destroyed unless it's associated page has been closed or is invalid.
        assert(page.get().map { !$0.hasRunningProcess() } ?? (currentIndex == nil))
    }

    func getMessageReceiver() -> RefWebBackForwardListMessageForwarder {
        // Guaranteed to be Some after construction
        // swift-format-ignore: NeverForceUnwrap
        self.messageForwarder!
    }

    func itemForID(identifier: WebCore.BackForwardItemIdentifier) -> WebKit.WebBackForwardListItem? {
        // FIXME: consider restructuring this a bit. It's a bit odd that it basically refers
        // to a map within WebBackForwardListItem. Maybe WebBackForwardList should
        // own that map. This is a pre-existing quirk of the C++ implementation, not a
        // new Swift thing.
        guard let page = page.get() else {
            return nil
        }

        // FIXME(rdar://162357139): We can't use == here
        assert(
            WebKit.WebBackForwardListItem.itemForID(identifier) == nil
                || contentsMatch(WebKit.WebBackForwardListItem.itemForID(identifier).pageID(), page.identifier())
        )
        return WebKit.WebBackForwardListItem.itemForID(identifier)
    }

    func pageClosed() {
        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) had its page closed with current size \(entries.count)")

        // We should have always started out with an m_page and we should never close the page twice
        let pageAvailable = page.__convertToBool()
        assert(pageAvailable)
        if pageAvailable {
            for item in entries {
                didRemoveItem(item: item)
            }
        }

        page.clear()
        entries.removeAll()
        currentIndex = nil
    }

    private func assertValidIndex() {
        assert(currentIndex.map { $0 < entries.count } ?? true)
    }

    func addItem(newItem: WebKit.WebBackForwardListItem) {
        assertValidIndex()

        guard let page = page.get() else {
            return
        }

        var removedItems: [WebKit.WebBackForwardListItem] = []

        if let initialCurrentIndex = currentIndex {
            page.recordAutomaticNavigationSnapshot()

            // Toss everything in the forward list.
            let targetSize = initialCurrentIndex + 1
            removedItems.reserveCapacity(entries.count - targetSize)

            while entries.count > targetSize {
                // swift-format-ignore: NeverForceUnwrap
                didRemoveItem(item: entries.last!)
                removedItems.append(entries.removeLast())
            }

            // Toss the first item if the list is getting too big, as long as we're not using it
            // (or even if we are, if we only want 1 entry).
            // swift-format-ignore: NeverForceUnwrap
            if entries.count >= WebBackForwardList.defaultCapacity && currentIndex! > 0 {
                // swift-format-ignore: NeverForceUnwrap
                didRemoveItem(item: entries.first!)
                removedItems.append(entries.removeFirst())

                if entries.isEmpty {
                    currentIndex = nil
                } else {
                    // swift-format-ignore: NeverForceUnwrap
                    currentIndex = currentIndex! - 1
                }
            }
        } else {
            // If we have no current item index we should also not have any entries.
            // (This in future could be proven by using a Swift enum.)
            assert(entries.isEmpty)

            // But just in case it does happen in practice we'll get back in to a consistent state now before adding the new item.
            for item in entries {
                didRemoveItem(item: item)
            }
            removedItems.append(contentsOf: entries)
            entries.removeAll()
        }

        var shouldKeepCurrentItem = true

        if let initialCurrentIndex = currentIndex {
            shouldKeepCurrentItem = page.shouldKeepCurrentBackForwardListItemInList(entries[initialCurrentIndex])
            if shouldKeepCurrentItem {
                currentIndex = initialCurrentIndex + 1
            }
        } else {
            assert(entries.isEmpty)
            currentIndex = 0
        }

        // swift-format-ignore: NeverForceUnwrap
        let unwrappedCurrentIndex = currentIndex!
        if !shouldKeepCurrentItem {
            // m_current should never be pointing past the end of the entries Vector.
            // If it is, something has gone wrong and we should not try to swap in the new item.
            assert(unwrappedCurrentIndex < entries.count)
            removedItems.append(entries[unwrappedCurrentIndex])
            entries[unwrappedCurrentIndex] = newItem
        } else {
            // m_current should never be pointing more than 1 past the end of the entries Vector.
            // If it is, something has gone wrong and we should not try to insert the new item.
            assert(unwrappedCurrentIndex <= entries.count)
            if unwrappedCurrentIndex <= entries.count {
                entries.insert(newItem, at: unwrappedCurrentIndex)
            }
        }

        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) added an item. Current size \(entries.count), current index \(String(describing: currentIndex)), threw away \(removedItems.count) items"
        )
        page.didChangeBackForwardList(newItem, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    func goToItem(item: WebKit.WebBackForwardListItem) {
        assertValidIndex()

        guard !entries.isEmpty else {
            return
        }
        guard let page = page.get() else {
            return
        }
        guard let priorCurrentIndex = currentIndex else {
            return
        }

        let targetIndex = entries.firstIndex(where: { $0 === item })

        // If the target item wasn't even in the list, there's nothing else to do.
        guard var targetIndex else {
            backForwardLog(
                "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) could not go to item \(item.identifier().toString()) \(item.url) because it was not found"
            )
            return
        }

        if targetIndex < priorCurrentIndex {
            let delta = entries.count - targetIndex - 1
            let deltaValue = if delta > 10 { "over10" } else { delta.description }
            page.logDiagnosticMessage(
                WebCore.DiagnosticLoggingKeys.backNavigationDeltaKey(),
                WTF.String(deltaValue),
                WebCore.ShouldSample.No
            )
        }

        // If we're going to an item different from the current item, ask the client if the current
        // item should remain in the list.
        let currentItem = entries[priorCurrentIndex]
        var shouldKeepCurrentItem = true
        if !(currentItem === item) {
            page.recordAutomaticNavigationSnapshot()
            shouldKeepCurrentItem = page.shouldKeepCurrentBackForwardListItemInList(currentItem)
        }

        // If the client said to remove the current item, remove it and then update the target index.
        var removedItems: [WebKit.WebBackForwardListItem] = []
        if !shouldKeepCurrentItem {
            removedItems.append(entries.remove(at: priorCurrentIndex))
            // swift-format-ignore: NeverForceUnwrap
            targetIndex = entries.firstIndex(where: { $0 === item })!
        }

        currentIndex = targetIndex

        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) going to item \(item.identifier().toString()), is now at index \(targetIndex)"
        )
        page.didChangeBackForwardList(Optional.none, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    func currentItem() -> WebKit.WebBackForwardListItem? {
        assertValidIndex()

        guard page.__convertToBool() else {
            return nil
        }

        guard let currentIndex = currentIndex else {
            return nil
        }

        return entries[currentIndex]
    }

    func backItem() -> WebKit.WebBackForwardListItem? {
        assertValidIndex()

        guard page.__convertToBool() else {
            return nil
        }

        guard let currentIndex = currentIndex else {
            return nil
        }

        guard currentIndex > 0 else {
            return nil
        }
        return entries[currentIndex - 1]
    }

    func forwardItem() -> WebKit.WebBackForwardListItem? {
        assertValidIndex()

        guard page.__convertToBool() else {
            return nil
        }

        guard let currentIndex = currentIndex else {
            return nil
        }

        guard currentIndex < entries.count - 1 else {
            return nil
        }
        return entries[currentIndex + 1]
    }

    func itemAtIndex(index: Array.Index) -> WebKit.WebBackForwardListItem? {
        assertValidIndex()

        guard page.__convertToBool() else {
            return nil
        }

        guard let currentIndex = currentIndex else {
            return nil
        }

        // Do range checks without doing math on index to avoid overflow.
        if index < 0 && -index > backListCount() {
            return nil
        }

        if index > 0 && index > forwardListCount() {
            return nil
        }

        return entries[index + currentIndex]
    }

    func backListCount() -> Array.Index {
        assertValidIndex()

        guard page.__convertToBool() else {
            return 0
        }

        guard let currentIndex = currentIndex else {
            return 0
        }

        return currentIndex
    }

    func forwardListCount() -> Array.Index {
        assertValidIndex()

        guard page.__convertToBool() else {
            return 0
        }

        guard let currentIndex = currentIndex else {
            return 0
        }

        return entries.count - (currentIndex + 1)
    }

    private func counts() -> WebKit.WebBackForwardListCounts {
        WebKit.WebBackForwardListCounts(backCount: UInt32(backListCount()), forwardCount: UInt32(forwardListCount()))
    }

    func backListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
        assertValidIndex()

        guard page.__convertToBool() else {
            return API.Array.create()
        }

        if currentIndex == nil {
            return API.Array.create()
        }

        let backListSize = backListCount()
        let size = min(backListSize, Int(limit))
        guard size > 0 else {
            return API.Array.create()
        }
        assert(backListSize >= size)
        let startIndex = backListSize - size

        return API.Array.create(list: entries[startIndex..<startIndex + size].map { WebKit.toAPIObject($0) })
    }

    func forwardListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
        assertValidIndex()

        guard page.__convertToBool() else {
            return API.Array.create()
        }

        guard let unwrappedCurrentIndex = currentIndex else {
            return API.Array.create()
        }

        let size = min(forwardListCount(), Int(limit))
        guard size > 0 else {
            return API.Array.create()
        }
        let startIndex = unwrappedCurrentIndex + 1
        return API.Array.create(list: entries[startIndex..<startIndex + size].map { WebKit.toAPIObject($0) })
    }

    func removeAllItems() {
        assertValidIndex()

        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) removeAllItems (has \(entries.count) of them)")

        for item in entries {
            didRemoveItem(item: item)
        }
        currentIndex = nil

        let entriesCopy = entries
        entries.removeAll()
        // swift-format-ignore: NeverForceUnwrap
        page.get()!.didChangeBackForwardList(Optional.none, consuming: WebKit.BackForwardListItemVector(array: entriesCopy))
    }

    func clear() {
        assertValidIndex()

        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) clear (has \(entries.count) of them)")

        let size = entries.count
        guard let page = page.get() else {
            return
        }
        guard size > 1 else {
            return
        }

        guard let unwrappedCurrentItem = currentItem() else {
            // We should only ever have no current item if we also have no current item index.
            assert(currentIndex == nil)

            // But just in case it does happen in practice we should get back into a consistent state now.
            removeAllItems()
            return
        }

        for item in entries where !(item === unwrappedCurrentItem) {
            didRemoveItem(item: item)
        }

        var removedItems: [WebKit.WebBackForwardListItem] = []
        removedItems.reserveCapacity(size - 1)

        if let unwrappedCurrentIndex = currentIndex {
            for (i, entry) in entries.enumerated() where i != unwrappedCurrentIndex {
                removedItems.append(entry)
            }
        }

        currentIndex = 0
        entries.removeAll()
        entries.append(unwrappedCurrentItem)
        page.didChangeBackForwardList(nil, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    func backForwardListState(filter: WebBackForwardListItemFilter) -> WebKit.BackForwardListState {
        assertValidIndex()

        var backForwardListState = WebKit.BackForwardListState.init()
        if let currentIndex = currentIndex {
            setOptionalUInt32Value(&backForwardListState.currentIndex, UInt32(currentIndex))
        }

        for (i, entry) in entries.enumerated() {
            if filterSpecified(filter) && !callFilter(filter, entry) {
                if let stateCurrentIndex = Optional(fromCxx: backForwardListState.currentIndex) {
                    if i < stateCurrentIndex && stateCurrentIndex != 0 {
                        setOptionalUInt32Value(&backForwardListState.currentIndex, stateCurrentIndex - 1)
                    }
                }
                continue
            }
            backForwardListState.items.append(
                consuming: WebKit.BackForwardListItemState(
                    frameState: entry.copyMainFrameStateWithChildren(),
                    navigatedFrameID: entry.navigatedFrameID()
                )
            )
        }

        if backForwardListState.items.isEmpty() {
            backForwardListState.currentIndex = nil
        } else if let currentIndex = Optional(fromCxx: backForwardListState.currentIndex) {
            if backForwardListState.items.size() <= currentIndex {
                setOptionalUInt32Value(&backForwardListState.currentIndex, UInt32(backForwardListState.items.size()) - 1)
            }
        }
        return backForwardListState
    }

    private func setBackForwardItemIdentifiers(frameState: WebKit.FrameState, itemID: WebCore.BackForwardItemIdentifier) {
        frameState.itemID = WebCore.MarkableBackForwardItemIdentifier(itemID)
        frameState.frameItemID = WebCore.MarkableBackForwardFrameItemIdentifier(generateBackForwardFrameItemIdentifier())
        for child in CxxVectorIterator(vec: frameState.children) {
            setBackForwardItemIdentifiers(frameState: child.ptr(), itemID: itemID)
        }
    }

    func restoreFromState(backForwardListState: WebKit.BackForwardListState) {
        guard let page = page.get() else {
            return
        }

        // FIXME: Enable restoring resourceDirectoryURL.
        entries.removeAll()
        entries.reserveCapacity(backForwardListState.items.size())
        for itemState in CxxVectorIterator(vec: backForwardListState.items) {
            let stateCopy = itemState.frameState.ptr().copy()
            setBackForwardItemIdentifiers(frameState: stateCopy.ptr(), itemID: generateBackForwardItemIdentifier())
            let item = WebKit.WebBackForwardListItem.create(consuming: stateCopy, page.identifier(), itemState.navigatedFrameID)
            entries.append(item.ptr())
        }

        currentIndex = Optional(fromCxx: backForwardListState.currentIndex).map({ val in Int(val) })
        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) restored from state (has \(entries.count) entries)")
    }

    func setItemsAsRestoredFromSession() {
        for entry in entries {
            entry.setWasRestoredFromSession()
        }
    }

    func setItemsAsRestoredFromSessionIf(functor: WebBackForwardListItemFilter) {
        for entry in entries where callFilter(functor, entry) {
            entry.setWasRestoredFromSession()
        }
    }

    func didRemoveItem(item: WebKit.WebBackForwardListItem) {
        item.wasRemovedFromBackForwardList()
        // swift-format-ignore: NeverForceUnwrap
        page.get()!.backForwardRemovedItem(item.mainFrameItem().identifier())

        // rdar://168139870 to clean up use of BUILDING_GTK__ here.
        #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS) || os(visionOS) || BUILDING_GTK__
        item.setSnapshot(consuming: WebKit.RefPtrViewSnapshot())
        #endif
    }

    func goBackItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
        itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(direction: Direction.backward)
    }

    func goForwardItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
        itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(direction: Direction.forward)
    }

    private func itemSkippingBackForwardItemsAddedByJSWithoutUserGesture(direction: Direction) -> WebKit.RefPtrWebBackForwardListItem {
        let delta =
            switch direction {
            case .backward: -1
            case .forward: 1
            }
        var itemIndex = delta
        let item = itemAtIndex(index: itemIndex)
        guard var item = item else {
            return WebKit.RefPtrWebBackForwardListItem()
        }

        #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS) || os(visionOS)
        if !WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.UIBackForwardSkipsHistoryItemsWithoutUserGesture) {
            return WebKit.RefPtrWebBackForwardListItem(item)
        }
        #endif

        // For example:
        // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
        // If we're on Google and navigate back, we don't want to skip anything and load Yahoo#a.
        // However, if we're on Yahoo and navigate forward, we do want to skip items and end up on Google#b.
        // swift-format-ignore: NeverForceUnwrap
        if direction == Direction.backward && !currentItem()!.wasCreatedByJSWithoutUserInteraction() {
            return WebKit.RefPtrWebBackForwardListItem(item)
        }

        // For example:
        // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
        // If we are on Google#b and navigate backwards, we want to skip over Google#a and Google, to end up on Yahoo#a.
        // If we are on Yahoo#a and navigate forwards, we want to skip over Google and Google#a, to end up on Google#b.
        let originalItem = item
        while item.wasCreatedByJSWithoutUserInteraction() {
            itemIndex += delta
            let thisItem = itemAtIndex(index: itemIndex)
            guard let thisItem else {
                return WebKit.RefPtrWebBackForwardListItem(originalItem)
            }
            item = thisItem

            loadingReleaseLog(
                "UI Navigation is skipping a WebBackForwardListItem because it was added by JavaScript without user interaction"
            )
        }

        // We are now on the next item that has user interaction.
        assert(!item.wasCreatedByJSWithoutUserInteraction())

        if direction == Direction.backward {
            // If going backwards, skip over next item with user iteraction since this is the one the user
            // thinks they're on.
            itemIndex -= 1
            let thisItem = itemAtIndex(index: itemIndex)
            guard let thisItem else {
                return WebKit.RefPtrWebBackForwardListItem(originalItem)
            }
            item = thisItem

            loadingReleaseLog(
                "UI Navigation is skipping a WebBackForwardListItem that has user interaction because we started on an item that didn't have interaction"
            )
        } else {
            // If going forward and there are items that we created by JS without user interaction, move forward to the last
            // one in the series.
            var nextItem = itemAtIndex(index: itemIndex + 1)
            while let unwrappedNextItem = nextItem, unwrappedNextItem.wasCreatedByJSWithoutUserInteraction() {
                item = unwrappedNextItem
                itemIndex += 1
                nextItem = itemAtIndex(index: itemIndex + 1)
            }
        }
        return WebKit.RefPtrWebBackForwardListItem(item)
    }

    func findFrameStateInItem(
        itemID: WebCore.BackForwardItemIdentifier,
        parentFrameID: WebCore.FrameIdentifier,
        childFrameIndex: UInt64
    ) -> WebKit.FrameState? {
        guard let targetItem = itemForID(identifier: itemID) else {
            return nil
        }
        guard let parentFrameItem = targetItem.mainFrameItem().childItemForFrameID(parentFrameID) else {
            return nil
        }
        guard let childFrameItem = parentFrameItem.childItemAtIndex(childFrameIndex) else {
            return nil
        }
        return childFrameItem.frameState()
    }

    func loggingString() -> Swift.String {
        var result =
            "\nWebBackForwardList \(ObjectIdentifier(self)) - \(entries.count) entries, has current index \(currentIndex != nil ? "YES" : "NO") (\(currentIndex ?? 0))\n"

        for (i, entry) in entries.enumerated() {
            let prefix = (currentIndex == i) ? " * " : " - "
            result += prefix + String(entry.loggingString().description)
        }

        return result
    }

    private func addChildItem(parentFrameID: WebCore.FrameIdentifier, frameState: WebKit.RefFrameState) {
        guard let currentItem = currentItem() else {
            return
        }
        guard let parentItem = currentItem.mainFrameItem().childItemForFrameID(parentFrameID) else {
            return
        }
        parentItem.setChild(consuming: frameState)
    }

    func setBackForwardItemIdentifier(frameState: WebKit.FrameState, itemID: WebCore.BackForwardItemIdentifier) {
        frameState.itemID = WebCore.MarkableBackForwardItemIdentifier(itemID)
        for child in CxxVectorIterator(vec: frameState.children) {
            setBackForwardItemIdentifier(frameState: child.ptr(), itemID: itemID)
        }
    }

    func completeFrameStateForNavigation(navigatedFrameState: WebKit.FrameState) -> WebKit.FrameState {
        guard let currentItem = currentItem() else {
            return navigatedFrameState
        }
        guard let navigatedFrameID = Optional(fromCxx: navigatedFrameState.frameID) else {
            return navigatedFrameState
        }
        let mainFrameItem = currentItem.mainFrameItem()
        if let mainFrameID = Optional(fromCxx: mainFrameItem.frameID()) {
            if contentsMatch(mainFrameID, navigatedFrameID) {
                return navigatedFrameState
            }
        }

        if mainFrameItem.childItemForFrameID(navigatedFrameID) == nil {
            return navigatedFrameState
        }
        let frameState = currentItem.copyMainFrameStateWithChildren().ptr()
        setBackForwardItemIdentifier(frameState: frameState, itemID: navigatedFrameState.itemID.pointee)
        frameState.replaceChildFrameState(consuming: WebKit.RefFrameState(navigatedFrameState))
        return frameState
    }

    func backForwardAddItemShared(
        connection: IPC.Connection,
        navigatedFrameState: WebKit.RefFrameState,
        loadedWebArchive: WebKit.LoadedWebArchive
    ) {
        let process = WebKit.WebProcessProxy.fromConnection(connection)

        // 'nil' works around rdar://162310543
        // Safety: it's OK to pass a null pointer to these two functions; in fact it's the default
        let itemURL = unsafe WTF.URL(navigatedFrameState.ptr().urlString, nil)
        let itemOriginalURL = unsafe WTF.URL(navigatedFrameState.ptr().originalURLString, nil)

        #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS) || os(visionOS)
        #if os(macOS)
        let doMessageChecks =
            WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.PushStateFilePathRestriction)
            && !WTF.MacApplication.isMimeoPhotoProject()
        #else
        let doMessageChecks =
            WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.PushStateFilePathRestriction)
        #endif
        if doMessageChecks { // corresponds to the first 'if' condition in C++ WebBackForwardList::backForwardAddItemShared
            assert(!itemURL.protocolIsFile() || process.ptr().wasPreviouslyApprovedFileURL(itemURL))
            if messageCheck(
                process: process,
                !itemURL.protocolIsFile() || process.ptr().wasPreviouslyApprovedFileURL(itemURL)
            ) {
                return
            }
            if messageCheck(
                process: process,
                !itemOriginalURL.protocolIsFile() || process.ptr().wasPreviouslyApprovedFileURL(itemOriginalURL)
            ) {
                return
            }
        }
        #endif

        let navigatedFrameID = navigatedFrameState.ptr().frameID
        let targetFrame = WebKit.WebFrameProxy.webFrame(navigatedFrameID)

        guard let targetFrame else {
            return
        }

        if targetFrame.isPendingInitialHistoryItem() {
            targetFrame.setIsPendingInitialHistoryItem(false)
            if let parent = targetFrame.parentFrame() {
                addChildItem(parentFrameID: parent.frameID(), frameState: navigatedFrameState)
            }
            return
        }

        guard let webPageProxy = page.get() else {
            return
        }

        let item = WebKit.WebBackForwardListItem
            .create(
                consuming: WebKit.RefFrameState(completeFrameStateForNavigation(navigatedFrameState: navigatedFrameState.ptr())),
                webPageProxy.identifier(),
                navigatedFrameID,
                webPageProxy.browsingContextGroup()
            )
            .ptr()
        item.setResourceDirectoryURL(consuming: webPageProxy.currentResourceDirectoryURL())
        item.setEnhancedSecurity(process.ptr().enhancedSecurity())
        if loadedWebArchive == WebKit.LoadedWebArchive.Yes {
            item.setDataStoreForWebArchive(process.ptr().websiteDataStore())
        }
        addItem(newItem: item)
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
            assert(WebKit.downcastToWebProcessProxy(process).__convertToBool())
            let hasBackForwardCacheEntry = item.backForwardCacheEntry() != nil
            if hasBackForwardCacheEntry != frameState.ptr().hasCachedPage {
                if frameState.ptr().hasCachedPage {
                    webPageProxy.backForwardCache().addEntry(item, process.coreProcessIdentifier())
                } else if item.suspendedPage() == nil {
                    webPageProxy.backForwardCache().removeEntry(item)
                }
            }
        }
        let oldFrameID = frameItem.frameID()
        frameItem.setFrameState(consuming: frameState)
        let newFrameID = frameItem.frameID()
        if let oldFrameID = Optional(fromCxx: oldFrameID) {
            if let newFrameID = Optional(fromCxx: newFrameID) {
                if !contentsMatch(oldFrameID, newFrameID) {
                    updateFrameIdentifier(oldFrameID: oldFrameID, newFrameID: newFrameID)
                }
            }
        }
    }

    func updateFrameIdentifier(oldFrameID: WebCore.FrameIdentifier, newFrameID: WebCore.FrameIdentifier) {
        for entry in entries {
            entry.updateFrameID(oldFrameID, newFrameID)
        }
    }

    func backForwardGoToItem(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardGoToItemCompletionHandler
    ) {
        // On process swap, we tell the previous process to ignore the load, which causes it to restore its current back forward item to its previous
        // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
        // Any real new load in the committed process would have cleared m_provisionalPage.
        if let webPageProxy = page.get(), webPageProxy.hasProvisionalPage() {
            callCompletionHandler(completionHandler, consuming: counts())
            return
        }

        backForwardGoToItemShared(itemID: itemID, completionHandler: completionHandler)
    }

    func backForwardListContainsItem(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardListContainsItemCompletionHandler
    ) {
        callCompletionHandler(completionHandler, itemForID(identifier: itemID) != nil)
    }

    func backForwardGoToItemShared(
        itemID: WebCore.BackForwardItemIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardGoToItemCompletionHandler
    ) {
        if let webPageProxy = page.get() {
            if messageCheckCompletion(
                process: WebKit.RefWebProcessProxy(webPageProxy.legacyMainFrameProcess()),
                completionHandler: { callCompletionHandler(completionHandler, consuming: counts()) },
                !WebKit.isInspectorPage(webPageProxy)
            ) {
                return
            }
        }

        if let item = itemForID(identifier: itemID) {
            goToItem(item: item)
        }

        callCompletionHandler(completionHandler, consuming: counts())
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
        callCompletionHandler(completionHandler, consuming: WebKit.VectorRefFrameState(array: frameStates))
    }

    func backForwardItemAtIndex(
        index: Int32,
        frameID: WebCore.FrameIdentifier,
        completionHandler: CompletionHandlers.WebBackForwardList.BackForwardItemAtIndexCompletionHandler
    ) {
        // FIXME: This should verify that the web process requesting the item hosts the specified frame.
        let index = Int(index)
        guard let item = itemAtIndex(index: index) else {
            callCompletionHandler(completionHandler, consuming: WebKit.RefPtrFrameState())
            return
        }
        guard let frameItem = item.mainFrameItem().childItemForFrameID(frameID) else {
            callCompletionHandler(completionHandler, consuming: WebKit.RefPtrFrameState(item.copyMainFrameStateWithChildren().ptr()))
            return
        }
        callCompletionHandler(completionHandler, consuming: WebKit.RefPtrFrameState(frameItem.copyFrameStateWithChildren().ptr()))
    }

    func backForwardListCounts(completionHandler: CompletionHandlers.WebBackForwardList.BackForwardListCountsCompletionHandler) {
        callCompletionHandler(completionHandler, consuming: counts())
    }
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

#endif // compiler(>=6.2.3)

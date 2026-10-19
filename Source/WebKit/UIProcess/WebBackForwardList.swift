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

// FIXME(rdar://130765784): We should be able use the built-in ===, but AnyObject currently excludes foreign reference types
@_expose(!Cxx) // rdar://169474185
func === (_ lhs: WebKit.WebBackForwardListItem, _ rhs: WebKit.WebBackForwardListItem) -> Bool {
    // Safety: Swift represents all reference types, including foreign reference types, as raw pointers
    unsafe unsafeBitCast(lhs, to: UnsafeRawPointer.self) == unsafeBitCast(rhs, to: UnsafeRawPointer.self)
}

private typealias Item = WebKit.WebBackForwardListItem

final class WebBackForwardList {
    private static let defaultCapacity = 100

    private var page: WebKit.WeakPtrWebPageProxy
    private lazy var messageForwarder = WebKit.WebBackForwardListMessageForwarder.create(target: self)

    /// `nil` when the list is empty.
    private var entries: Entries?

    // Set while dispatching an IPC message coming from a provisional (process-swapped) process, where the
    // message may legitimately reference an unexpected file: URL. See setHandlingProvisionalMessage(_:).
    private var handlingProvisionalMessage = false

    private enum Direction {
        case backward
        case forward

        var step: Int {
            switch self {
            case .backward: -1
            case .forward: 1
            }
        }
    }

    /// The list split around its current item.
    ///
    /// There's no index and `current` isn't optional, so "has a current item" and "the current index
    /// is in bounds" are always true.
    private struct Entries: RandomAccessCollection {
        var back: [Item] = []
        var current: Item
        var forward: [Item] = []

        init(current: Item) {
            self.current = current
        }

        /// Splits `items` around `currentIndex`, or returns `nil` if `currentIndex` isn't an index of `items`.
        init?(_ items: [Item], currentIndex: Int?) {
            guard let currentIndex, items.indices.contains(currentIndex) else {
                return nil
            }
            back = Array(items[..<currentIndex])
            current = items[currentIndex]
            forward = Array(items[(currentIndex + 1)...])
        }

        var currentIndex: Int { back.count }

        // RandomAccessCollection, so the skipping code can index the whole list without copying it.
        var startIndex: Int { 0 }
        var endIndex: Int { back.count + 1 + forward.count }

        subscript(index: Int) -> Item {
            if index < back.count {
                return back[index]
            }
            if index == back.count {
                return current
            }
            return forward[index - back.count - 1]
        }

        enum Position: Equatable {
            case back(Int)
            case current
            case forward(Int)
        }

        func position(of item: Item) -> Position? {
            if item === current {
                return .current
            }
            if let index = back.firstIndex(where: { $0 === item }) {
                return .back(index)
            }
            if let index = forward.firstIndex(where: { $0 === item }) {
                return .forward(index)
            }
            return nil
        }

        /// Makes the item at `position` current.
        ///
        /// If `keepingCurrent` is false, the old current item is dropped.
        mutating func move(to position: Position, keepingCurrent: Bool) {
            let oldCurrent = keepingCurrent ? [current] : []
            switch position {
            case .current:
                return
            case .back(let index):
                forward = Array(back[(index + 1)...]) + oldCurrent + forward
                current = back[index]
                back.removeSubrange(index...)
            case .forward(let index):
                back += oldCurrent + forward[..<index]
                current = forward[index]
                forward.removeSubrange(...index)
            }
        }

        /// The nearest item to the current one in `direction`, skipping items that were added by JavaScript
        /// without a user gesture.
        func itemSkippingItemsWithoutUserGesture(_ direction: Direction) -> Item? {
            indexSkippingItemsWithoutUserGesture(direction, from: currentIndex).map { self[$0] }
        }

        /// Up to `limit` items from the current one in `direction`, nearest first, skipping items that were
        /// added by JavaScript without a user gesture.
        func itemsSkippingItemsWithoutUserGesture(_ direction: Direction, limit: Int) -> [Item] {
            var items: [Item] = []
            var index = currentIndex
            while items.count < limit, let nextIndex = indexSkippingItemsWithoutUserGesture(direction, from: index) {
                items.append(self[nextIndex])
                index = nextIndex
            }
            return items
        }

        /// The index of the item one step from `startingIndex` in `direction`, skipping items that were
        /// added by JavaScript without a user gesture.
        private func indexSkippingItemsWithoutUserGesture(_ direction: Direction, from startingIndex: Int) -> Int? {
            assert(indices.contains(startingIndex))
            var index = startingIndex + direction.step
            guard indices.contains(index) else {
                return nil
            }

            #if WTF_PLATFORM_COCOA
            if !WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.UIBackForwardSkipsHistoryItemsWithoutUserGesture) {
                return index
            }
            #endif

            // For example:
            // A -> A#a (no userInteraction) -> B -> B#a (no user interaction) -> B#b (no user interaction)
            // If we're on B and navigate back, we don't want to skip anything and load A#a.
            // However, if we're on A and navigate forward, we do want to skip items and end up on B#b.
            // The forward logic comes later.
            if direction == .backward && !self[startingIndex].wasCreatedByJSWithoutUserInteraction() {
                return index
            }

            // If every item from this point back to the start of the list was created by JS without user interaction,
            // we ignore them all.
            if direction == .backward && self[...index].allSatisfy({ $0.wasCreatedByJSWithoutUserInteraction() }) {
                return nil
            }

            // For example:
            // Yahoo -> Yahoo#a (no userInteraction) -> Google -> Google#a (no user interaction) -> Google#b (no user interaction)
            // If we are on Google#b and navigate backwards, we want to skip over Google#a and Google, to end up on Yahoo#a.
            // If we are on Yahoo#a and navigate forwards, we want to skip over Google and Google#a, to end up on Google#b.
            while self[index].wasCreatedByJSWithoutUserInteraction() {
                index += direction.step
                guard indices.contains(index) else {
                    // If there are no more back items that ever had a user gesture, then we should not enable going back.
                    // This happens when e.g. a new window is created by JavaScript then client redirects occur that create
                    // a sequence of history items, each without user interaction.
                    loadingReleaseLog(
                        "UI Navigation is disabling going back because no more WebBackForwardListItem items in the back list had user interaction"
                    )
                    return nil
                }

                loadingReleaseLog(
                    "UI Navigation is skipping a WebBackForwardListItem because it was added by JavaScript without user interaction"
                )
            }

            // We are now on the next item that has user interaction.
            assert(!self[index].wasCreatedByJSWithoutUserInteraction())

            switch direction {
            case .backward:
                // If going backwards, skip over next item with user iteraction since this is the one the user
                // thinks they're on. But if the user-gesture item is at the start of history, there is nothing
                // to skip to — the item itself must be the destination.
                guard index > 0 else {
                    return index
                }

                loadingReleaseLog(
                    "UI Navigation is skipping a WebBackForwardListItem that has user interaction because we started on an item that didn't have interaction"
                )
                return index - 1
            case .forward:
                // If going forward and there are items that we created by JS without user interaction, move forward to the last
                // one in the series.
                while indices.contains(index + 1) && self[index + 1].wasCreatedByJSWithoutUserInteraction() {
                    index += 1
                }
                return index
            }
        }
    }

    private static let shouldSkipItemsWithoutUserGestureForWebKitAPI: Bool = {
        #if WTF_PLATFORM_COCOA
        return WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.AllBackForwardItemsWithoutUserGestureInvisibleToUI)
        #else
        return false
        #endif
    }()

    // @used ensures these are retained even under -O -wmo: rdar://179098545
    @used
    init(page: WebKit.WeakPtrWebPageProxy) {
        self.page = page
        backForwardLog("(Back/Forward) Created WebBackForwardList \(ObjectIdentifier(self))")
    }

    deinit {
        backForwardLog("(Back/Forward) Destroying WebBackForwardList \(ObjectIdentifier(self))")
        // A WebBackForwardList should never be destroyed unless it's associated page has been closed or is invalid.
        assert(page.get().map { !$0.hasRunningProcess() } ?? (entries == nil))
    }

    @used
    func getMessageReceiver() -> RefWebBackForwardListMessageForwarder {
        messageForwarder
    }

    /// The entries, if the page is alive.
    private var liveEntries: Entries? {
        page.get() != nil ? entries : nil
    }

    /// Every item in order, for code that doesn't care about the current position.
    private var allItems: [Item] {
        entries.map { Array($0) } ?? []
    }

    @used
    func itemForID(identifier: WebCore.BackForwardItemIdentifier) -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(entryForID(identifier))
    }

    private func entryForID(_ identifier: WebCore.BackForwardItemIdentifier) -> Item? {
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

    @used
    func pageClosed() {
        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) had its page closed with current size \(entries?.count ?? 0)"
        )

        // We should have always started out with an m_page and we should never close the page twice
        if let page = page.get() {
            for item in allItems {
                didRemoveItem(item, from: page)
            }
        } else {
            assertionFailure("WebBackForwardList should have a page, and should not be closed twice")
        }

        page.clear()
        entries = nil
    }

    private func addItem(_ newItem: Item) {
        guard let page = page.get() else {
            return
        }

        var removedItems: [Item] = []
        let newEntries: Entries
        if var entries = self.entries {
            page.recordAutomaticNavigationSnapshot()

            // Toss everything in the forward list (last first, matching the C++).
            removedItems = entries.forward.reversed()
            entries.forward.removeAll()

            // Toss the first item if the list is getting too big, as long as we're not using it.
            if entries.count >= Self.defaultCapacity, !entries.back.isEmpty {
                removedItems.append(entries.back.removeFirst())
            }
            for item in removedItems {
                didRemoveItem(item, from: page)
            }

            // The client can read the list from inside shouldKeepCurrentBackForwardListItemInList,
            // so store the trimmed list first.
            self.entries = entries

            if page.shouldKeepCurrentBackForwardListItemInList(entries.current) {
                entries.back.append(entries.current)
            } else {
                removedItems.append(entries.current)
            }
            entries.current = newItem
            newEntries = entries
        } else {
            newEntries = Entries(current: newItem)
        }
        self.entries = newEntries

        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) added an item. Current size \(newEntries.count), current index \(newEntries.currentIndex), threw away \(removedItems.count) items"
        )
        page.didChangeBackForwardList(newItem, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    @used
    func goToItem(item: WebKit.WebBackForwardListItem) {
        guard let page = page.get(), var entries = self.entries else {
            return
        }

        // If the target item wasn't even in the list, there's nothing else to do.
        guard let position = entries.position(of: item) else {
            backForwardLog(
                "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) could not go to item \(item.identifier().toString()) \(item.url) because it was not found"
            )
            return
        }

        if case .back(let index) = position {
            let delta = entries.count - index - 1
            let deltaValue = if delta > 10 { "over10" } else { delta.description }
            page.logDiagnosticMessage(
                WebCore.DiagnosticLoggingKeys.backNavigationDeltaKey(),
                WTF.String(deltaValue),
                WebCore.ShouldSample.No
            )
        }

        // If we're going to an item different from the current item, ask the client if the current
        // item should remain in the list.
        let shouldKeepCurrentItem: Bool
        if position == .current {
            shouldKeepCurrentItem = true
        } else {
            page.recordAutomaticNavigationSnapshot()
            shouldKeepCurrentItem = page.shouldKeepCurrentBackForwardListItemInList(entries.current)
        }

        let removedItems = shouldKeepCurrentItem ? [] : [entries.current]
        entries.move(to: position, keepingCurrent: shouldKeepCurrentItem)
        self.entries = entries

        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) going to item \(item.identifier().toString()), is now at index \(entries.currentIndex)"
        )
        page.didChangeBackForwardList(nil, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    @used
    func currentItem() -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(currentEntry())
    }

    private func currentEntry() -> Item? {
        liveEntries?.current
    }

    @used
    func backItem() -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(backEntry())
    }

    private func backEntry() -> Item? {
        guard let entries = liveEntries else {
            return nil
        }

        if Self.shouldSkipItemsWithoutUserGestureForWebKitAPI {
            return entries.itemSkippingItemsWithoutUserGesture(.backward)
        }
        return entries.back.last
    }

    @used
    func forwardItem() -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(forwardEntry())
    }

    private func forwardEntry() -> Item? {
        guard let entries = liveEntries else {
            return nil
        }

        if Self.shouldSkipItemsWithoutUserGestureForWebKitAPI {
            return entries.itemSkippingItemsWithoutUserGesture(.forward)
        }
        return entries.forward.first
    }

    @used
    func itemAtDeltaFromCurrentIndex(delta: Int, allowSkipping: Bool = true) -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(entryAtDeltaFromCurrentIndex(delta: delta, allowSkipping: allowSkipping))
    }

    private func entryAtDeltaFromCurrentIndex(delta: Int, allowSkipping: Bool) -> Item? {
        guard let entries = liveEntries else {
            return nil
        }

        let (targetIndex, overflow) = entries.currentIndex.addingReportingOverflow(delta)
        guard !overflow, targetIndex >= 0 else {
            return nil
        }

        // API requests to get the current item will always get the current item without any skipping logic.
        guard delta != 0 else {
            return entries.current
        }

        guard allowSkipping && Self.shouldSkipItemsWithoutUserGestureForWebKitAPI else {
            return entries.indices.contains(targetIndex) ? entries[targetIndex] : nil
        }

        let steps = abs(delta)
        let items = entries.itemsSkippingItemsWithoutUserGesture(delta < 0 ? .backward : .forward, limit: steps)
        return items.count == steps ? items.last : nil
    }

    private func rawBackListEntryCount() -> Int {
        liveEntries?.back.count ?? 0
    }

    private func rawForwardListEntryCount() -> Int {
        liveEntries?.forward.count ?? 0
    }

    @used
    func backListCountForAPI() -> Int {
        backListItems(limit: .max).count
    }

    @used
    func forwardListCountForAPI() -> Int {
        forwardListItems(limit: .max).count
    }

    private func rawCounts() -> WebKit.WebBackForwardListCounts {
        WebKit.WebBackForwardListCounts(backCount: UInt32(rawBackListEntryCount()), forwardCount: UInt32(rawForwardListEntryCount()))
    }

    private static func makeAPIArray(_ items: [Item]) -> API.RefAPIArray {
        API.Array.create(list: items.map { WebKit.toAPIObject($0) })
    }

    @used
    func backListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
        Self.makeAPIArray(backListItems(limit: Int(limit)))
    }

    @used
    func forwardListAsAPIArrayWithLimit(limit: UInt) -> API.RefAPIArray {
        Self.makeAPIArray(forwardListItems(limit: Int(limit)))
    }

    private func backListItems(limit: Int) -> [Item] {
        guard let entries = liveEntries else {
            return []
        }

        if Self.shouldSkipItemsWithoutUserGestureForWebKitAPI {
            return entries.itemsSkippingItemsWithoutUserGesture(.backward, limit: limit).reversed()
        }
        return Array(entries.back.suffix(limit))
    }

    private func forwardListItems(limit: Int) -> [Item] {
        guard let entries = liveEntries else {
            return []
        }

        if Self.shouldSkipItemsWithoutUserGestureForWebKitAPI {
            return entries.itemsSkippingItemsWithoutUserGesture(.forward, limit: limit)
        }
        return Array(entries.forward.prefix(limit))
    }

    @used
    func removeAllItems() {
        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) removeAllItems (has \(entries?.count ?? 0) of them)")

        guard let page = page.get() else {
            return
        }

        let removedItems = allItems
        for item in removedItems {
            didRemoveItem(item, from: page)
        }
        entries = nil

        // Clients are notified even if the list was already empty.
        page.didChangeBackForwardList(nil, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    @used
    func clear() {
        backForwardLog("(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) clear (has \(entries?.count ?? 0) of them)")

        guard let page = page.get(), let entries, entries.count > 1 else {
            return
        }

        // In list order: back items, then forward items.
        let removedItems = entries.back + entries.forward
        for item in removedItems {
            didRemoveItem(item, from: page)
        }

        self.entries = Entries(current: entries.current)
        page.didChangeBackForwardList(nil, consuming: WebKit.BackForwardListItemVector(array: removedItems))
    }

    @used
    func backForwardListState(filter: WebBackForwardListItemFilter) -> WebKit.BackForwardListState {
        var backForwardListState = WebKit.BackForwardListState()
        if let currentIndex = entries?.currentIndex {
            setOptionalUInt32Value(&backForwardListState.currentIndex, UInt32(currentIndex))
        }

        for (i, entry) in allItems.enumerated() {
            if filter.pointee.__convertToBool() && !filter.pointee(entry) {
                if let stateCurrentIndex = Optional(fromCxx: backForwardListState.currentIndex) {
                    if i <= stateCurrentIndex && stateCurrentIndex != 0 {
                        setOptionalUInt32Value(&backForwardListState.currentIndex, stateCurrentIndex - 1)
                    }
                }
                continue
            }
            appendToBackForwardStateItems(&backForwardListState.items, entry)
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

    @used
    func restoreFromState(backForwardListState: WebKit.BackForwardListState) {
        guard let page = page.get() else {
            return
        }

        // FIXME: Enable restoring resourceDirectoryURL.
        let restoredItems = createItemsFromState(backForwardListState, page.identifier())
        let items = (0..<restoredItems.size()).map { itemAtIndexInBackForwardListItemVector(restoredItems, $0) }
        entries = Entries(items, currentIndex: Optional(fromCxx: backForwardListState.currentIndex).map { Int($0) })

        backForwardLog(
            "(Back/Forward) WebBackForwardList \(ObjectIdentifier(self)) restored from state (has \(entries?.count ?? 0) entries)"
        )
    }

    @used
    func setItemsAsRestoredFromSession() {
        for entry in allItems {
            entry.setWasRestoredFromSession()
        }
    }

    @used
    func setItemsAsRestoredFromSessionIf(functor: WebBackForwardListItemFilter) {
        for entry in allItems where functor.pointee(entry) {
            entry.setWasRestoredFromSession()
        }
    }

    private func didRemoveItem(_ item: Item, from page: WebKit.WebPageProxy) {
        item.wasRemovedFromBackForwardList()
        page.backForwardRemovedItem(item.mainFrameItem().identifier())

        // rdar://168139870 to clean up use of BUILDING_GTK__ here.
        #if WTF_PLATFORM_COCOA || BUILDING_GTK__
        item.setSnapshot(consuming: WebKit.RefPtrViewSnapshot())
        #endif
    }

    @used
    func goBackItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(entries?.itemSkippingItemsWithoutUserGesture(.backward))
    }

    @used
    func goForwardItemSkippingItemsWithoutUserGesture() -> WebKit.RefPtrWebBackForwardListItem {
        WebKit.RefPtrWebBackForwardListItem(entries?.itemSkippingItemsWithoutUserGesture(.forward))
    }

    @used
    func findFrameStateInItem(
        itemID: WebCore.BackForwardItemIdentifier,
        parentFrameID: WebCore.FrameIdentifier,
        childFrameID: WebCore.FrameIdentifier,
        childFrameIndex: UInt64,
        childFrameName: WTF.String
    ) -> WebKit.RefPtrFrameState {
        guard let targetItem = entryForID(itemID) else {
            return WebKit.RefPtrFrameState()
        }
        // FIXME: After session restore, the back/forward list's frame identifiers don't match
        // the current WebView's frames because the original identifiers are unavailable.
        // Fall back to the mainFrameItem if the parentFrameID isn't found.
        // This only works correctly for direct children of the main frame; nested frames
        // (e.g., subframe > nestedframe) will get the wrong FrameState.
        let parentFrameItem = targetItem.mainFrameItem().childItemForFrameID(parentFrameID) ?? targetItem.mainFrameItem()
        var childFrameItem = parentFrameItem.childItemForFrameID(childFrameID)
        if childFrameItem == nil {
            // The identifier is absent after session restore or cross-site child-frame recreation
            if childFrameName.isEmpty() {
                childFrameItem = parentFrameItem.childItemAtIndex(childFrameIndex)
            } else {
                childFrameItem = parentFrameItem.childItemForFrameName(childFrameName)
            }
            guard let matchedItem = childFrameItem else {
                return WebKit.RefPtrFrameState()
            }
            let existingFrameID = Optional(fromCxx: matchedItem.frameID())
            if existingFrameID == nil {
                matchedItem.updateFrameID(childFrameID)
            }
        }
        guard let childFrameItem else {
            return WebKit.RefPtrFrameState()
        }
        return WebKit.RefPtrFrameState(getFrameState(childFrameItem))
    }

    @used
    func loggingString() -> Swift.String {
        let currentIndex = entries?.currentIndex
        var result =
            "\nWebBackForwardList \(ObjectIdentifier(self)) - \(entries?.count ?? 0) entries, has current index \(currentIndex != nil ? "YES" : "NO") (\(currentIndex ?? 0))\n"

        for (i, entry) in allItems.enumerated() {
            let prefix = (currentIndex == i) ? " * " : " - "
            result += prefix + String(entry.loggingString().description)
        }

        return result
    }

    private func addChildItem(parentFrameID: WebCore.FrameIdentifier, frameState: WebKit.RefFrameState) {
        guard let currentItem = currentEntry() else {
            return
        }
        guard let parentItem = currentItem.mainFrameItem().childItemForFrameID(parentFrameID) else {
            return
        }
        parentItem.setChild(consuming: frameState)
    }

    func setBackForwardItemIdentifier(frameState: WebKit.FrameState, itemID: WebCore.BackForwardItemIdentifier) {
        setFrameStateBackForwardItemIdentifier(frameState, itemID)
    }

    private func completeFrameStateForNavigation(navigatedFrameState: WebKit.FrameState) -> WebKit.FrameState {
        guard let currentItem = currentEntry() else {
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
        let frameStateRef = currentItem.copyMainFrameStateWithChildren()
        let frameState = frameStateRef.ptr()
        setBackForwardItemIdentifier(frameState: frameState, itemID: navigatedFrameState.itemID.pointee)
        frameState.replaceChildFrameState(consuming: WebKit.RefFrameState(navigatedFrameState))
        return frameState
    }

    private func messageCheckItemURLs(
        frameState: WebKit.RefFrameState,
        process: WebKit.RefWebProcessProxy,
        depth: UInt32 = 0
    ) throws(InvalidMessage) {
        try messageCheck { depth < maxFrameStateDepthForMessageCheck() }

        // 'nil' works around rdar://162310543
        // Safety: it's OK to pass a null pointer to these two functions; in fact it's the default
        let itemURL = unsafe WTF.URL(frameState.ptr().urlString, nil)
        let itemOriginalURL = unsafe WTF.URL(frameState.ptr().originalURLString, nil)

        #if WTF_PLATFORM_COCOA
        #if WTF_PLATFORM_MAC
        let doMessageChecks =
            WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.PushStateFilePathRestriction)
            && !WTF.MacApplication.isMimeoPhotoProject()
        #else
        let doMessageChecks =
            WTF.linkedOnOrAfterSDKWithBehavior(WTF.SDKAlignedBehavior.PushStateFilePathRestriction)
        #endif
        if doMessageChecks { // corresponds to the first 'if' condition in C++ messageCheckItemURLs
            try messageCheck {
                !itemURL.protocolIsFile() || process.ptr().wasPreviouslyApprovedFileURL(itemURL)
            }
            try messageCheck {
                !itemOriginalURL.protocolIsFile() || process.ptr().wasPreviouslyApprovedFileURL(itemOriginalURL)
            }
        }
        #endif

        let childCount = frameStateChildCount(frameState.ptr())
        for i in 0..<childCount {
            let child = frameStateChildAtIndex(frameState.ptr(), i)
            try messageCheckItemURLs(frameState: WebKit.RefFrameState(child), process: process, depth: depth + 1)
        }
    }

    @used
    func backForwardAddItemShared(
        connection: IPC.Connection,
        navigatedFrameState: WebKit.RefFrameState,
        loadedWebArchive: WebKit.LoadedWebArchive,
        browsingContextGroup: WebKit.BrowsingContextGroup
    ) {
        // Also reached from C++ (WebPageProxy::backForwardAddItemShared), so this rather than
        // the caller is the catch site.
        dispatchMessage(on: connection) { () throws(InvalidMessage) in
            try addItemInternal(
                connection: connection,
                navigatedFrameState: navigatedFrameState,
                loadedWebArchive: loadedWebArchive,
                browsingContextGroup: browsingContextGroup
            )
        }
    }

    private func addItemInternal(
        connection: IPC.Connection,
        navigatedFrameState: WebKit.RefFrameState,
        loadedWebArchive: WebKit.LoadedWebArchive,
        browsingContextGroup: WebKit.BrowsingContextGroup
    ) throws(InvalidMessage) {
        let process = WebKit.WebProcessProxy.fromConnection(connection)

        #if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
        let hasItemID = Bool(fromCxx: navigatedFrameState.ptr().itemID)
        let hasFrameItemID = Bool(fromCxx: navigatedFrameState.ptr().frameItemID)
        #else
        let hasItemID = navigatedFrameState.ptr().itemID.__convertToBool()
        let hasFrameItemID = navigatedFrameState.ptr().frameItemID.__convertToBool()
        #endif

        try messageCheck {
            !hasItemID
                || contentsMatch(navigatedFrameState.ptr().itemID.pointee.processIdentifier(), process.ptr().coreProcessIdentifier())
        }
        try messageCheck {
            !hasFrameItemID
                || contentsMatch(
                    navigatedFrameState.ptr().frameItemID.pointee.processIdentifier(),
                    process.ptr().coreProcessIdentifier()
                )
        }

        try messageCheckItemURLs(frameState: navigatedFrameState, process: process)

        let navigatedFrameID = navigatedFrameState.ptr().frameID
        let targetFrame = WebKit.WebFrameProxy.webFrame(navigatedFrameID)

        guard let targetFrame else {
            return
        }

        let listPage = page.get()
        let framePage = targetFrame.page()
        let pagesMatch: Bool
        if let framePage, let listPage {
            // We can't use == here due to rdar://162357139
            pagesMatch = contentsMatch(framePage.identifier(), listPage.identifier())
        } else {
            pagesMatch = framePage == nil && listPage == nil
        }

        try messageCheck { pagesMatch }

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
                browsingContextGroup
            )
            .ptr()
        item.setResourceDirectoryURL(consuming: webPageProxy.currentResourceDirectoryURL())
        item.setEnhancedSecurity(process.ptr().enhancedSecurity())
        if loadedWebArchive == WebKit.LoadedWebArchive.Yes {
            item.setDataStoreForWebArchive(process.ptr().websiteDataStore())
        }
        addItem(item)
    }

    // IPCs from here on

    // The Swift counterpart of WebBackForwardList::didReceiveProvisionalMessage in the C++ implementation.
    // ProvisionalPageProxy wraps its dispatch of provisional BackForwardUpdateItem messages with
    // setHandlingProvisionalMessage(true)/(false) so backForwardUpdateItem can skip the file: URL message check.
    @used
    func setHandlingProvisionalMessage(_ handling: Bool) {
        handlingProvisionalMessage = handling
    }

    @MainActor
    func backForwardAddItem(connection: IPC.Connection, navigatedFrameState: WebKit.RefFrameState) throws(InvalidMessage) {
        if let page = page.get() {
            try addItemInternal(
                connection: connection,
                navigatedFrameState: navigatedFrameState,
                loadedWebArchive: page.didLoadWebArchive() ? .Yes : .No,
                browsingContextGroup: page.browsingContextGroup()
            )
        }
    }

    @MainActor
    func backForwardSetChildItem(
        connection: IPC.Connection,
        frameItemID: WebCore.BackForwardFrameItemIdentifier,
        frameState: WebKit.RefFrameState
    ) throws(InvalidMessage) {
        let process = WebKit.WebProcessProxy.fromConnection(connection)
        try messageCheckItemURLs(frameState: frameState, process: process)

        guard let item = currentEntry() else {
            return
        }

        if let frameItem = WebKit.WebBackForwardListFrameItem.itemForID(item.identifier(), frameItemID) {
            frameItem.setChild(consuming: frameState)
        }
    }

    @MainActor
    func backForwardClearChildren(
        connection: IPC.Connection,
        itemID: WebCore.BackForwardItemIdentifier,
        frameItemID: WebCore.BackForwardFrameItemIdentifier
    ) {
        if let frameItem = WebKit.WebBackForwardListFrameItem.itemForID(itemID, frameItemID) {
            frameItem.clearChildren()
        }
    }

    @MainActor
    func backForwardUpdateItem(connection: IPC.Connection, frameState: WebKit.RefFrameState) throws(InvalidMessage) {
        let process = WebKit.WebProcessProxy.fromConnection(connection)

        // In the case of a process swap, the `backForwardUpdateItem` message can be received from the old process,
        // and therefore present an unexpected file: URL.
        // We can safely skip the message check in these cases.
        if !handlingProvisionalMessage {
            try messageCheckItemURLs(frameState: frameState, process: process)
        }

        #if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN
        let hasItemID = Bool(fromCxx: frameState.ptr().itemID)
        let hasFrameItemID = Bool(fromCxx: frameState.ptr().frameItemID)
        #else
        let hasItemID = frameState.ptr().itemID.__convertToBool()
        let hasFrameItemID = frameState.ptr().frameItemID.__convertToBool()
        #endif

        guard hasItemID && hasFrameItemID else {
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
        try messageCheck {
            contentsMatch(webPageProxy.identifier(), item.pageID()) && contentsMatch(itemID, item.identifier())
        }
        let oldFrameID = frameItem.frameID()
        frameItem.updateFrameStatePayload(consuming: frameState)
        let newFrameID = frameItem.frameID()
        if let oldFrameID = Optional(fromCxx: oldFrameID) {
            if let newFrameID = Optional(fromCxx: newFrameID) {
                if !contentsMatch(oldFrameID, newFrameID) {
                    updateFrameIdentifier(oldFrameID: oldFrameID, newFrameID: newFrameID)
                }
            }
        }

        webPageProxy.updateCanGoBackAndForward()
    }

    @used
    func updateFrameIdentifier(oldFrameID: WebCore.FrameIdentifier, newFrameID: WebCore.FrameIdentifier) {
        for entry in allItems {
            entry.updateFrameID(oldFrameID, newFrameID)
        }
    }

    @used
    func replaceFrameStateForChild(
        item: WebKit.WebBackForwardListItem,
        frameID: WebCore.FrameIdentifier,
        newFrameState: WebKit.RefFrameState
    ) {
        guard let targetFrameItem = item.mainFrameItem().childItemForFrameID(frameID) else {
            return
        }

        targetFrameItem.updateFrameStatePayload(consuming: newFrameState)
    }

    @MainActor
    func backForwardGoToItem(connection: IPC.Connection, itemID: WebCore.BackForwardItemIdentifier) throws(InvalidMessage) {
        // On process swap, we tell the previous process to ignore the load, which causes it to restore its current back forward item to its previous
        // value. Since the load is really going on in a new provisional process, we want to ignore such requests from the committed process.
        // Any real new load in the committed process would have cleared m_provisionalPage.
        if let webPageProxy = page.get(), webPageProxy.hasProvisionalPage() {
            return
        }

        try goToItemInternal(itemID: itemID)
    }

    @MainActor
    func backForwardListContainsItem(
        connection: IPC.Connection,
        itemID: WebCore.BackForwardItemIdentifier
    ) -> Bool {
        entryForID(itemID) != nil
    }

    @used
    // Also reached from C++ (WebPageProxy::backForwardGoToItemShared), so this rather than the
    // caller is the catch site.
    func backForwardGoToItemShared(connection: IPC.Connection, itemID: WebCore.BackForwardItemIdentifier) {
        do {
            try goToItemInternal(itemID: itemID)
        } catch {
            markMessageInvalid(error, on: connection)
        }
    }

    private func goToItemInternal(itemID: WebCore.BackForwardItemIdentifier) throws(InvalidMessage) {
        if let webPageProxy = page.get() {
            try messageCheck { !WebKit.isInspectorPage(webPageProxy) }
        }

        if let item = entryForID(itemID) {
            // Mirror of the C++ backForwardGoToItemShared guard (webkit.org/b/318728): ignore an index
            // move opposite to the in-flight traversal direction so a stale split leg can't clobber it.
            if let webPageProxy = page.get(), let position = entries?.position(of: item) {
                let direction = webPageProxy.inFlightTraversalDirection()
                switch position {
                case .back where direction > 0, .forward where direction < 0:
                    return
                default:
                    break
                }
            }
            goToItem(item: item)
        }
    }

    // Not a message check: a subframe process may name the main frame, which it does not host, and a
    // frame can be destroyed while a synchronous request naming it is in flight.
    private func frameOnPage(_ frameID: WebCore.FrameIdentifier) -> WebKit.WebFrameProxy? {
        guard let webPageProxy = page.get(), let frame = webFrameForFrameID(frameID), let framePage = frame.page() else {
            return nil
        }
        // We can't use == here due to rdar://162357139
        guard contentsMatch(framePage.identifier(), webPageProxy.identifier()) else {
            return nil
        }
        return frame
    }

    @MainActor
    func backForwardAllItems(
        connection: IPC.Connection,
        frameID: WebCore.FrameIdentifier
    ) -> WebKit.VectorRefFrameState {
        guard frameOnPage(frameID) != nil else {
            return WebKit.VectorRefFrameState(array: [])
        }
        var frameStates: [WebKit.FrameState] = []
        for item in allItems {
            if let frameItem = item.mainFrameItem().childItemForFrameID(frameID) {
                frameStates.append(frameItem.copyFrameStateWithChildren().ptr())
            }
        }
        return WebKit.VectorRefFrameState(array: frameStates)
    }

    @MainActor
    func backForwardItemAtIndexForWebContent(
        connection: IPC.Connection,
        delta: Int32,
        frameID: WebCore.FrameIdentifier
    ) throws(InvalidMessage) -> WebKit.RefPtrFrameState {
        try itemAtIndexForWebContent(delta: delta, frameID: frameID)
    }

    private func itemAtIndexForWebContent(
        delta: Int32,
        frameID: WebCore.FrameIdentifier
    ) throws(InvalidMessage) -> WebKit.RefPtrFrameState {
        try messageCheck { delta != Int32.min }

        guard let frame = frameOnPage(frameID) else {
            return WebKit.RefPtrFrameState()
        }

        let delta = Int(delta)
        guard let item = entryAtDeltaFromCurrentIndex(delta: delta, allowSkipping: false) else {
            return WebKit.RefPtrFrameState()
        }
        guard let frameItem = item.mainFrameItem().childItemForFrameID(frameID) else {
            // Entries can lack the main frame's current ID (after session restore or a process swap).
            guard frame.isMainFrame() else {
                return WebKit.RefPtrFrameState()
            }
            return WebKit.RefPtrFrameState(item.copyMainFrameStateWithChildren().ptr())
        }
        return WebKit.RefPtrFrameState(frameItem.copyFrameStateWithChildren().ptr())
    }

    @MainActor
    func backForwardListCounts(
        connection: IPC.Connection
    ) -> WebKit.WebBackForwardListCounts {
        rawCounts()
    }
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

#endif // compiler(>=6.2.3)

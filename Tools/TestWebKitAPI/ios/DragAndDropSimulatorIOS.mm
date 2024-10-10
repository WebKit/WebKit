/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "DragAndDropSimulator.h"

#if ENABLE(DRAG_SUPPORT) && PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "UIKitSPIForTesting.h"
#import <UIKit/UIDragInteraction.h>
#import <UIKit/UIDragItem.h>
#import <UIKit/UIDropInteraction.h>
#import <UIKit/UIInteraction.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFocusedElementInfo.h>
#import <WebKit/_WKFormInputSession.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

#if USE(BROWSERENGINEKIT)
#import <BrowserEngineKit/BrowserEngineKit.h>
#endif

using namespace TestWebKitAPI;

@interface DragAndDropSimulator () <UIDragAnimating>

- (void)_setNeedsDropPreviewUpdate:(UIDragItem *)item;

@end

@interface UIDragItem (DragAndDropSimulator)
@property (nonatomic, strong, setter=_setDragAndDropSimulator:) DragAndDropSimulator *_dragAndDropSimulator;
@end

static char dragDropSimulatorKey;

@implementation UIDragItem (DragAndDropSimulator)

- (DragAndDropSimulator *)_dragAndDropSimulator
{
    return objc_getAssociatedObject(self, &dragDropSimulatorKey);
}

- (void)_setDragAndDropSimulator:(DragAndDropSimulator *)simulator
{
    objc_setAssociatedObject(self, &dragDropSimulatorKey, simulator, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)_setNeedsDropPreviewUpdate
{
    [self._dragAndDropSimulator _setNeedsDropPreviewUpdate:self];
}

@end

@implementation WKWebView (DragAndDropTesting)

- (UIView *)_dragDropInteractionView
{
    return [self valueForKey:@"_currentContentView"];
}

- (id<UIDropInteractionDelegate>)dropInteractionDelegate
{
    return (id<UIDropInteractionDelegate>)self._dragDropInteractionView;
}

template<typename InteractionType>
InteractionType *findInteractionOfType(UIView *view)
{
    for (id<UIInteraction> interaction in view.interactions) {
        if (auto foundInteraction = dynamic_objc_cast<InteractionType>(interaction))
            return foundInteraction;
    }
    return nil;
}

#if USE(BROWSERENGINEKIT)

- (id<BEDragInteractionDelegate>)dragInteractionDelegate
{
    return (id<BEDragInteractionDelegate>)self._dragDropInteractionView;
}

#else

- (id<UIDragInteractionDelegate>)dragInteractionDelegate
{
    return (id<UIDragInteractionDelegate>)self._dragDropInteractionView;
}

#endif

- (UIDropInteraction *)dropInteraction
{
    return findInteractionOfType<UIDropInteraction>(self._dragDropInteractionView);
}

- (id)dragInteraction
{
    return findInteractionOfType<UIDragInteraction>(self._dragDropInteractionView);
}

@end

@implementation MockDragDropSession

- (instancetype)initWithItems:(NSArray <UIDragItem *>*)items location:(CGPoint)locationInWindow window:(UIWindow *)window allowMove:(BOOL)allowMove
{
    if (self = [super init]) {
        _mockItems = items;
        _mockLocationInWindow = locationInWindow;
        _window = window;
        _allowMove = allowMove;
    }
    return self;
}

- (BOOL)allowsMoveOperation
{
    return _allowMove;
}

- (BOOL)isRestrictedToDraggingApplication
{
    return NO;
}

- (BOOL)hasItemsConformingToTypeIdentifiers:(NSArray<NSString *> *)typeIdentifiers
{
    for (NSString *typeIdentifier in typeIdentifiers) {
        BOOL hasItemConformingToType = NO;
        for (UIDragItem *item in self.items)
            hasItemConformingToType |= [[item.itemProvider registeredTypeIdentifiers] containsObject:typeIdentifier];
        if (!hasItemConformingToType)
            return NO;
    }
    return YES;
}

- (BOOL)canLoadObjectsOfClass:(Class<NSItemProviderReading>)aClass
{
    for (UIDragItem *item in self.items) {
        if ([item.itemProvider canLoadObjectOfClass:aClass])
            return YES;
    }
    return NO;
}

- (BOOL)canLoadObjectsOfClasses:(NSArray<Class<NSItemProviderReading>> *)classes
{
    for (Class<NSItemProviderReading> aClass in classes) {
        BOOL canLoad = NO;
        for (UIDragItem *item in self.items)
            canLoad |= [item.itemProvider canLoadObjectOfClass:aClass];
        if (!canLoad)
            return NO;
    }
    return YES;
}

- (NSArray<UIDragItem *> *)items
{
    return _mockItems.get();
}

- (void)setItems:(NSArray<UIDragItem *> *)items
{
    _mockItems = items;
}

- (void)addItems:(NSArray<UIDragItem *> *)items
{
    if (![items count])
        return;

    if (![_mockItems count])
        _mockItems = items;
    else
        _mockItems = [_mockItems arrayByAddingObjectsFromArray:items];
}

- (CGPoint)locationInView:(UIView *)view
{
    return [_window convertPoint:_mockLocationInWindow toView:view];
}

@end

@implementation MockDropSession

- (instancetype)initWithProviders:(NSArray<NSItemProvider *> *)providers location:(CGPoint)locationInWindow window:(UIWindow *)window allowMove:(BOOL)allowMove
{
    auto items = adoptNS([[NSMutableArray alloc] init]);
    for (NSItemProvider *itemProvider in providers)
        [items addObject:adoptNS([[UIDragItem alloc] initWithItemProvider:itemProvider]).get()];

    self = [super initWithItems:items.get() location:locationInWindow window:window allowMove:allowMove];
    return self;
}

- (BOOL)isLocal
{
    return YES;
}

- (NSProgress *)progress
{
    return [NSProgress discreteProgressWithTotalUnitCount:100];
}

- (void)setProgressIndicatorStyle:(UIDropSessionProgressIndicatorStyle)progressIndicatorStyle
{
}

- (UIDropSessionProgressIndicatorStyle)progressIndicatorStyle
{
    return UIDropSessionProgressIndicatorStyleNone;
}

- (NSUInteger)operationMask
{
    return 0;
}

- (id <UIDragSession>)localDragSession
{
    return nil;
}

- (BOOL)hasItemsConformingToTypeIdentifier:(NSString *)typeIdentifier
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (BOOL)canCreateItemsOfClass:(Class<NSItemProviderReading>)aClass
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (NSProgress *)loadObjectsOfClass:(Class<NSItemProviderReading>)aClass completion:(void(^)(NSArray<__kindof id <NSItemProviderReading>> *objects))completion
{
    ASSERT_NOT_REACHED();
    return nil;
}

@end

@implementation MockDragSession {
    RetainPtr<id> _localContext;
}

- (instancetype)initWithWindow:(UIWindow *)window allowMove:(BOOL)allowMove
{
    self = [super initWithItems:@[] location:CGPointZero window:window allowMove:allowMove];
    return self;
}

- (NSUInteger)localOperationMask
{
    ASSERT_NOT_REACHED();
    return 0;
}

- (NSUInteger)externalOperationMask
{
    ASSERT_NOT_REACHED();
    return 0;
}

- (id)session
{
    return nil;
}

- (id)localContext
{
    return _localContext.get();
}

- (void)setLocalContext:(id)localContext
{
    _localContext = localContext;
}

@end

static double progressIncrementStep = 0.033;
static double progressTimeStep = 0.016;
static NSString *TestWebKitAPISimulateCancelAllTouchesNotificationName = @"TestWebKitAPISimulateCancelAllTouchesNotificationName";

static NSArray *dragAndDropEventNames()
{
    static NSArray *eventNames = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^() {
        eventNames = @[ @"dragenter", @"dragover", @"drop", @"dragleave", @"dragstart" ];
    });
    return eventNames;
}

@interface DragAndDropSimulatorApplication : UIApplication
@end

@implementation DragAndDropSimulatorApplication

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)_cancelAllTouches
{
    [[NSNotificationCenter defaultCenter] postNotificationName:TestWebKitAPISimulateCancelAllTouchesNotificationName object:nil];
}
IGNORE_WARNINGS_END

@end

@implementation DragAndDropSimulator {
    RetainPtr<TestWKWebView> _webView;
    RetainPtr<MockDragSession> _dragSession;
    RetainPtr<MockDropSession> _dropSession;
    RetainPtr<NSMutableArray> _observedEventNames;
    RetainPtr<NSArray> _externalItemProviders;
    RetainPtr<NSArray> _sourceItemProviders;
    CGRect _finalSelectionStartRect;
    CGPoint _startLocation;
    CGPoint _endLocation;
    CGRect _lastKnownDragCaretRect;

    RetainPtr<NSMutableDictionary<NSNumber *, NSValue *>>_remainingAdditionalItemRequestLocationsByProgress;
    RetainPtr<NSMutableArray<NSValue *>>_queuedAdditionalItemRequestLocations;
    RetainPtr<NSMutableArray> _liftPreviews;
    RetainPtr<NSMutableArray<UITargetedDragPreview *>> _cancellationPreviews;
    RetainPtr<NSMutableArray> _dropPreviews;
    RetainPtr<NSMutableArray> _delayedDropPreviews;
    RetainPtr<NSMutableArray> _defaultDropPreviewsForExternalItems;

    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removedAttachments;

    bool _hasStartedInputSession;
    double _currentProgress;
    bool _isDoneWithCurrentRun;
    DragAndDropPhase _phase;

    RetainPtr<UIDropProposal> _lastKnownDropProposal;

    BlockPtr<BOOL(_WKActivatedElementInfo *)> _showCustomActionSheetBlock;
    BlockPtr<NSArray *(NSItemProvider *, NSArray *, NSDictionary *)> _convertItemProvidersBlock;
    BlockPtr<NSArray *(id <UIDropSession>)> _overridePerformDropBlock;
    BlockPtr<UIDropOperation(UIDropOperation, id)> _overrideDragUpdateBlock;
    BlockPtr<void(BOOL, NSArray *)> _dropCompletionBlock;
    BlockPtr<void()> _sessionWillBeginBlock;
    Vector<BlockPtr<void(UIViewAnimatingPosition)>> _dropAnimationCompletionBlocks;
}

- (instancetype)initWithWebViewFrame:(CGRect)frame
{
    return [self initWithWebViewFrame:frame configuration:nil];
}

- (instancetype)initWithWebViewFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (configuration)
        return [self initWithWebView:adoptNS([[TestWKWebView alloc] initWithFrame:frame configuration:configuration]).get()];

    return [self initWithWebView:adoptNS([[TestWKWebView alloc] initWithFrame:frame]).get()];
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        _shouldEnsureUIApplication = NO;
        _shouldBecomeFirstResponder = YES;
        _shouldAllowMoveOperation = YES;
        _dropAnimationTiming = DropAnimationShouldFinishAfterHandlingDrop;
        [_webView setUIDelegate:self];
        [_webView _setInputDelegate:self];
        self.dragDestinationAction = WKDragDestinationActionAny & ~WKDragDestinationActionLoad;
    }
    return self;
}

- (void)dealloc
{
    if ([_webView UIDelegate] == self)
        [_webView setUIDelegate:nil];

    if ([_webView _inputDelegate] == self)
        [_webView _setInputDelegate:nil];

    [super dealloc];
}

- (void)_resetSimulatedState
{
    _phase = DragAndDropPhaseBeginning;
    _currentProgress = 0;
    _isDoneWithCurrentRun = false;
    _observedEventNames = adoptNS([[NSMutableArray alloc] init]);
    _insertedAttachments = adoptNS([[NSMutableArray alloc] init]);
    _removedAttachments = adoptNS([[NSMutableArray alloc] init]);
    _finalSelectionStartRect = CGRectNull;
    _dragSession = nil;
    _dropSession = nil;
    _lastKnownDropProposal = nil;
    _lastKnownDragCaretRect = CGRectZero;
    _remainingAdditionalItemRequestLocationsByProgress = nil;
    _queuedAdditionalItemRequestLocations = adoptNS([[NSMutableArray alloc] init]);
    _liftPreviews = adoptNS([[NSMutableArray alloc] init]);
    _dropPreviews = adoptNS([[NSMutableArray alloc] init]);
    _cancellationPreviews = adoptNS([[NSMutableArray alloc] init]);
    _delayedDropPreviews = adoptNS([[NSMutableArray alloc] init]);
    _hasStartedInputSession = false;
}

- (NSArray *)observedEventNames
{
    return _observedEventNames.get();
}

- (UIDropProposal *)lastKnownDropProposal
{
    return _lastKnownDropProposal.get();
}

- (void)simulateAllTouchesCanceled:(NSNotification *)notification
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
    _phase = DragAndDropPhaseCancelled;
    _currentProgress = 1;
    _isDoneWithCurrentRun = true;
    if (_dragSession)
        [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] session:_dragSession.get() didEndWithOperation:UIDropOperationCancel];
}

- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation
{
    [self runFrom:startLocation to:endLocation additionalItemRequestLocations:nil];
}

- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation additionalItemRequestLocations:(ProgressToCGPointValueMap)additionalItemRequestLocations
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self selector:@selector(simulateAllTouchesCanceled:) name:TestWebKitAPISimulateCancelAllTouchesNotificationName object:nil];

    if (_shouldEnsureUIApplication)
        UIApplicationInstantiateSingleton([DragAndDropSimulatorApplication class]);

    if (_shouldBecomeFirstResponder)
        [_webView becomeFirstResponder];

    [self _resetSimulatedState];

    if (additionalItemRequestLocations)
        _remainingAdditionalItemRequestLocationsByProgress = adoptNS([additionalItemRequestLocations mutableCopy]);

    for (NSString *eventName in dragAndDropEventNames()) {
        [_webView performAfterReceivingMessage:eventName action:[strongSelf = retainPtr(self), name = retainPtr(eventName)] {
            [strongSelf->_observedEventNames addObject:name.get()];
        }];
    }

    _startLocation = startLocation;
    _endLocation = endLocation;

    if (self.externalItemProviders.count) {
        _dropSession = adoptNS([[MockDropSession alloc] initWithProviders:self.externalItemProviders location:_startLocation window:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        _phase = DragAndDropPhaseBegan;
        [self _advanceProgress];
    } else {
        _dragSession = adoptNS([[MockDragSession alloc] initWithWindow:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        [_dragSession setMockLocationInWindow:_startLocation];
#if USE(BROWSERENGINEKIT)
        [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] prepareDragSession:_dragSession.get() completion:[strongSelf = retainPtr(self)] {
            if (strongSelf->_phase == DragAndDropPhaseCancelled)
                return NO;

            strongSelf->_phase = DragAndDropPhaseBeginning;
            [strongSelf _advanceProgress];
            return YES;
        }];
#else
        [(id<UIDragInteractionDelegate_SPI>)[_webView dragInteractionDelegate] _dragInteraction:[_webView dragInteraction] prepareForSession:_dragSession.get() completion:[strongSelf = retainPtr(self)] {
            if (strongSelf->_phase == DragAndDropPhaseCancelled)
                return;

            strongSelf->_phase = DragAndDropPhaseBeginning;
            [strongSelf _advanceProgress];
        }];
#endif
    }

    Util::run(&_isDoneWithCurrentRun);
    // Wait for any delayed drop previews to be delivered.
    [_webView waitForNextPresentationUpdate];
    [_webView clearMessageHandlers:dragAndDropEventNames()];
    [_webView waitForNextPresentationUpdate];

    auto contentView = [_webView textInputContentView];
    _finalSelectionStartRect = [contentView caretRectForPosition:contentView.selectedTextRange.start];

    [defaultCenter removeObserver:self];
}

- (RetainPtr<UITargetedDragPreview>)defaultDropPreviewForItemAtIndex:(NSUInteger)index
{
    RetainPtr<UITargetedDragPreview> defaultPreview;
    if ([_defaultDropPreviewsForExternalItems count] == [_dropSession items].count)
        return [_defaultDropPreviewsForExternalItems objectAtIndex:index];

    // Just fall back to an arbitrary non-null drag preview if the test didn't specify one.
    return adoptNS([[UITargetedDragPreview alloc] initWithView:_webView.get()]);
}

- (void)_setNeedsDropPreviewUpdate:(UIDragItem *)item
{
    auto itemIndex = [[_dropSession items] indexOfObject:item];
    if (itemIndex == NSNotFound)
        return;

    auto interaction = [_webView dropInteraction];
    auto defaultPreview = [self defaultDropPreviewForItemAtIndex:itemIndex];
    if (auto updatedPreview = [[_webView dropInteractionDelegate] dropInteraction:interaction previewForDroppingItem:item withDefault:defaultPreview.get()])
        [_delayedDropPreviews setObject:updatedPreview atIndexedSubscript:itemIndex];
}

- (void)_concludeDropAndPerformOperationIfNecessary
{
    _lastKnownDragCaretRect = [_webView _dragCaretRect];
    auto operation = [_lastKnownDropProposal operation];
    if (operation != UIDropOperationCancel && operation != UIDropOperationForbidden) {
        NSInteger dropPreviewIndex = 0;
        for (UIDragItem *item in [_dropSession items]) {
            item._dragAndDropSimulator = self;
            auto defaultPreview = [self defaultDropPreviewForItemAtIndex:dropPreviewIndex];

            UIDropInteraction *interaction = [_webView dropInteraction];
            [_dropPreviews addObject:[[_webView dropInteractionDelegate] dropInteraction:interaction previewForDroppingItem:item withDefault:defaultPreview.get()] ?: NSNull.null];
            [_delayedDropPreviews addObject:NSNull.null];
            ++dropPreviewIndex;
        }

        [self _expectNoDropPreviewsWithUnparentedContainerViews];

        [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] performDrop:_dropSession.get()];
        _phase = DragAndDropPhasePerformingDrop;

        for (UIDragItem *item in [_dropSession items])
            [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] item:item willAnimateDropWithAnimator:self];

        if (_dropAnimationTiming == DropAnimationShouldFinishBeforeHandlingDrop) {
            [_webView evaluateJavaScript:@"" completionHandler:^(id, NSError *) {
                // We need to at least ensure one round trip to the web process and back, to ensure that the UI process will have received any image placeholders
                // that were just inserted as a result of performing the drop. However, this is guaranteed to run before the UI process receives the drop completion
                // message, since item provider loading is asynchronous.
                [self _invokeDropAnimationCompletionBlocksAndConcludeDrop];
            }];
        }
    } else {
        _isDoneWithCurrentRun = true;
        _phase = DragAndDropPhaseCancelled;
        [[_dropSession items] enumerateObjectsUsingBlock:^(UIDragItem *item, NSUInteger index, BOOL *) {
            UITargetedDragPreview *defaultPreview = nil;
            if ([_liftPreviews count] && [[_liftPreviews objectAtIndex:index] isEqual:NSNull.null])
                defaultPreview = [_liftPreviews objectAtIndex:index];

            UITargetedDragPreview *preview = [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] previewForCancellingItem:item withDefault:defaultPreview];
            if (preview)
                [_cancellationPreviews addObject:preview];
        }];
        [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] concludeDrop:_dropSession.get()];
    }

    [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] sessionDidEnd:_dropSession.get()];

    if (_dragSession) {
        auto delegate = [_webView dragInteractionDelegate];
        [delegate dragInteraction:[_webView dragInteraction] session:_dragSession.get() didEndWithOperation:operation];
        if ([delegate respondsToSelector:@selector(_clearToken:)])
            [(id <UITextInputMultiDocument>)delegate _clearToken:nil];
        [_webView becomeFirstResponder];
    }
}

- (void)_enqueuePendingAdditionalItemRequestLocations
{
    NSMutableArray *progressValuesToRemove = [NSMutableArray array];
    for (NSNumber *progressValue in _remainingAdditionalItemRequestLocationsByProgress.get()) {
        double progress = progressValue.doubleValue;
        if (progress > _currentProgress)
            continue;
        [progressValuesToRemove addObject:progressValue];
        [_queuedAdditionalItemRequestLocations addObject:[_remainingAdditionalItemRequestLocationsByProgress objectForKey:progressValue]];
    }

    for (NSNumber *progressToRemove in progressValuesToRemove)
        [_remainingAdditionalItemRequestLocationsByProgress removeObjectForKey:progressToRemove];
}

- (BOOL)_sendQueuedAdditionalItemRequest
{
    if (![_queuedAdditionalItemRequestLocations count])
        return NO;

    RetainPtr<NSValue> requestLocationValue = [_queuedAdditionalItemRequestLocations objectAtIndex:0];
    [_queuedAdditionalItemRequestLocations removeObjectAtIndex:0];

    auto requestLocation = [[_webView window] convertPoint:[requestLocationValue CGPointValue] toView:_webView.get()];
#if USE(BROWSERENGINEKIT)
    [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] itemsForAddingToSession:_dragSession.get() forTouchAtPoint:requestLocation completion:[dragSession = _dragSession, dropSession = _dropSession](NSArray *items) {
        [dragSession addItems:items];
        [dropSession addItems:items];
        return YES;
    }];
#else
    [(id<UIDragInteractionDelegate_SPI>)[_webView dragInteractionDelegate] _dragInteraction:[_webView dragInteraction] itemsForAddingToSession:_dragSession.get() withTouchAtPoint:requestLocation completion:[dragSession = _dragSession, dropSession = _dropSession] (NSArray *items) {
        [dragSession addItems:items];
        [dropSession addItems:items];
    }];
#endif
    return YES;
}

- (void)_advanceProgress
{
    [self _enqueuePendingAdditionalItemRequestLocations];
    if ([self _sendQueuedAdditionalItemRequest]) {
        [self _scheduleAdvanceProgress];
        return;
    }

    _lastKnownDragCaretRect = [_webView _dragCaretRect];
    _currentProgress += progressIncrementStep;
    CGPoint locationInWindow = self._currentLocation;
    [_dragSession setMockLocationInWindow:locationInWindow];
    [_dropSession setMockLocationInWindow:locationInWindow];

    if (_currentProgress >= 1) {
        _currentProgress = 1;
        [self _concludeDropAndPerformOperationIfNecessary];
        return;
    }

    switch (_phase) {
    case DragAndDropPhaseBeginning: {
        NSMutableArray<NSItemProvider *> *itemProviders = [NSMutableArray array];
        NSArray *items = [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] itemsForBeginningSession:_dragSession.get()];
        if (!items.count) {
            _phase = DragAndDropPhaseCancelled;
            _currentProgress = 1;
            _isDoneWithCurrentRun = true;
            return;
        }

        for (UIDragItem *item in items) {
            [itemProviders addObject:item.itemProvider];
            UITargetedDragPreview *liftPreview = [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] previewForLiftingItem:item session:_dragSession.get()];
            EXPECT_TRUE(liftPreview || ![_webView window]);
            [_liftPreviews addObject:liftPreview ?: NSNull.null];
        }

        _dropSession = adoptNS([[MockDropSession alloc] initWithProviders:itemProviders location:self._currentLocation window:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        [_dragSession setItems:items];
        _sourceItemProviders = itemProviders;
        if (self.showCustomActionSheetBlock) {
            // Defer progress until the custom action sheet is dismissed.
            auto startLocationInView = [[_webView window] convertPoint:_startLocation toView:_webView.get()];
            [_webView _simulateLongPressActionAtLocation:startLocationInView];
            return;
        }

        auto delegate = [_webView dragInteractionDelegate];
        if ([delegate respondsToSelector:@selector(_preserveFocusWithToken:destructively:)])
            [(id <UITextInputMultiDocument>)delegate _preserveFocusWithToken:nil destructively:NO];

        [_webView resignFirstResponder];

        [delegate dragInteraction:[_webView dragInteraction] sessionWillBegin:_dragSession.get()];

        RetainPtr<WKWebView> retainedWebView = _webView;
        dispatch_async(dispatch_get_main_queue(), ^() {
            [retainedWebView resignFirstResponder];
        });

        _phase = DragAndDropPhaseBegan;
        break;
    }
    case DragAndDropPhaseBegan:
        [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] sessionDidEnter:_dropSession.get()];
        _phase = DragAndDropPhaseEntered;
        break;
    case DragAndDropPhaseEntered: {
        _lastKnownDropProposal = [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] sessionDidUpdate:_dropSession.get()];
        [_webView waitForNextPresentationUpdate];
        if (![self shouldAllowMoveOperation] && [_lastKnownDropProposal operation] == UIDropOperationMove)
            _lastKnownDropProposal = adoptNS([[UIDropProposal alloc] initWithDropOperation:UIDropOperationCancel]);
        break;
    }
    default:
        break;
    }

    [self _scheduleAdvanceProgress];
}

- (void)clearExternalDragInformation
{
    _externalItemProviders = nil;
    _defaultDropPreviewsForExternalItems = nil;
}

- (CGPoint)_currentLocation
{
    CGFloat distanceX = _endLocation.x - _startLocation.x;
    CGFloat distanceY = _endLocation.y - _startLocation.y;
    return CGPointMake(_startLocation.x + _currentProgress * distanceX, _startLocation.y + _currentProgress * distanceY);
}

- (void)_scheduleAdvanceProgress
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
    [self performSelector:@selector(_advanceProgress) withObject:nil afterDelay:progressTimeStep];
}

- (NSArray *)sourceItemProviders
{
    return _sourceItemProviders.get();
}

- (NSArray *)externalItemProviders
{
    return _externalItemProviders.get();
}

- (void)setExternalItemProviders:(NSArray *)externalItemProviders
{
    _externalItemProviders = adoptNS([externalItemProviders copy]);
}

- (void)setExternalItemProviders:(NSArray<NSItemProvider *> *)itemProviders defaultDropPreviews:(NSArray<UITargetedDragPreview *> *)previews
{
    ASSERT(itemProviders.count == previews.count);
    self.externalItemProviders = itemProviders;
    _defaultDropPreviewsForExternalItems = adoptNS(previews.copy);
}

- (DragAndDropPhase)phase
{
    return _phase;
}

- (NSArray *)liftPreviews
{
    return _liftPreviews.get();
}

- (NSArray<UITargetedDragPreview *> *)cancellationPreviews
{
    return _cancellationPreviews.get();
}

- (NSArray<UITargetedDragPreview *> *)dropPreviews
{
    return _dropPreviews.get();
}

- (NSArray<UITargetedDragPreview *> *)delayedDropPreviews
{
    return _delayedDropPreviews.get();
}

- (CGRect)lastKnownDragCaretRect
{
    return _lastKnownDragCaretRect;
}

- (void)ensureInputSession
{
    Util::run(&_hasStartedInputSession);
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return _insertedAttachments.get();
}

- (NSArray<_WKAttachment *> *)removedAttachments
{
    return _removedAttachments.get();
}

- (void)endDataTransfer
{
    [[_webView dragInteractionDelegate] dragInteraction:[_webView dragInteraction] sessionDidTransferItems:_dragSession.get()];
}

- (TestWKWebView *)webView
{
    return _webView.get();
}

- (void)setShowCustomActionSheetBlock:(BOOL(^)(_WKActivatedElementInfo *))showCustomActionSheetBlock
{
    _showCustomActionSheetBlock = showCustomActionSheetBlock;
}

- (BOOL(^)(_WKActivatedElementInfo *))showCustomActionSheetBlock
{
    return _showCustomActionSheetBlock.get();
}

- (void)setConvertItemProvidersBlock:(NSArray *(^)(NSItemProvider *, NSArray *, NSDictionary *))convertItemProvidersBlock
{
    _convertItemProvidersBlock = convertItemProvidersBlock;
}

- (NSArray *(^)(NSItemProvider *, NSArray *, NSDictionary *))convertItemProvidersBlock
{
    return _convertItemProvidersBlock.get();
}

- (void)setOverridePerformDropBlock:(NSArray *(^)(id <UIDropSession>))overridePerformDropBlock
{
    _overridePerformDropBlock = overridePerformDropBlock;
}

- (NSArray *(^)(id <UIDropSession>))overridePerformDropBlock
{
    return _overridePerformDropBlock.get();
}

- (void)setOverrideDragUpdateBlock:(UIDropOperation(^)(UIDropOperation, id <UIDropSession>))overrideDragUpdateBlock
{
    _overrideDragUpdateBlock = overrideDragUpdateBlock;
}

- (UIDropOperation(^)(UIDropOperation, id <UIDropSession>))overrideDragUpdateBlock
{
    return _overrideDragUpdateBlock.get();
}

- (void)setDropCompletionBlock:(void(^)(BOOL, NSArray *))dropCompletionBlock
{
    _dropCompletionBlock = dropCompletionBlock;
}

- (void(^)(BOOL, NSArray *))dropCompletionBlock
{
    return _dropCompletionBlock.get();
}

- (void)setSessionWillBeginBlock:(dispatch_block_t)block
{
    _sessionWillBeginBlock = block;
}

- (dispatch_block_t)sessionWillBeginBlock
{
    return _sessionWillBeginBlock.get();
}

- (void)addAnimations:(void (^)())animations
{
    // This is not implemented by the drag-and-drop simulator yet, since WebKit doesn't make use of
    // "alongside" animations during drop.
    ASSERT_NOT_REACHED();
}

- (void)addCompletion:(void (^)(UIViewAnimatingPosition))completion
{
    _dropAnimationCompletionBlocks.append(makeBlockPtr(completion));
}

- (void)_expectNoDropPreviewsWithUnparentedContainerViews
{
    auto checkDropPreview = [&](id dropPreviewOrNull) {
        if (![dropPreviewOrNull isKindOfClass:UITargetedPreview.class])
            return;

        auto *previewContainer = [(UITargetedDragPreview *)dropPreviewOrNull target].container;
        if (!previewContainer)
            return;

        if ([previewContainer isKindOfClass:UIWindow.class])
            return;

        EXPECT_NOT_NULL(previewContainer.window);
    };

    for (id dropPreviewOrNull in _dropPreviews.get())
        checkDropPreview(dropPreviewOrNull);

    for (id dropPreviewOrNull in _delayedDropPreviews.get())
        checkDropPreview(dropPreviewOrNull);
}

- (void)_invokeDropAnimationCompletionBlocksAndConcludeDrop
{
    [self _expectNoDropPreviewsWithUnparentedContainerViews];
    for (auto block : std::exchange(_dropAnimationCompletionBlocks, { }))
        block(UIViewAnimatingPositionEnd);
    [[_webView dropInteractionDelegate] dropInteraction:[_webView dropInteraction] concludeDrop:_dropSession.get()];
}

- (BOOL)containsDraggedType:(NSString *)expectedType
{
    for (NSItemProvider *itemProvider in self.sourceItemProviders) {
        for (NSString *type in itemProvider.registeredTypeIdentifiers) {
            if ([type isEqualToString:expectedType])
                return YES;
        }
    }
    return NO;
}

#pragma mark - WKUIDelegatePrivate

- (void)_webView:(WKWebView *)webView dataInteraction:(UIDragInteraction *)interaction sessionWillBegin:(id <UIDragSession>)session
{
    if (_sessionWillBeginBlock)
        _sessionWillBeginBlock();
}

- (void)_webView:(WKWebView *)webView dataInteractionOperationWasHandled:(BOOL)handled forSession:(id)session itemProviders:(NSArray<NSItemProvider *> *)itemProviders
{
    if (self.dropCompletionBlock)
        self.dropCompletionBlock(handled, itemProviders);

    if (_dropAnimationTiming == DropAnimationShouldFinishBeforeHandlingDrop) {
        _isDoneWithCurrentRun = true;
        return;
    }

    [_webView _doAfterReceivingEditDragSnapshotForTesting:^{
        [self _invokeDropAnimationCompletionBlocksAndConcludeDrop];
        _isDoneWithCurrentRun = true;
    }];
}

- (UIDropProposal *)_webView:(WKWebView *)webView willUpdateDropProposalToProposal:(UIDropProposal *)proposal forSession:(id <UIDropSession>)session
{
    if (!self.overrideDragUpdateBlock)
        return proposal;

    return adoptNS([[UIDropProposal alloc] initWithDropOperation:self.overrideDragUpdateBlock(proposal.operation, session)]).autorelease();
}

- (NSArray *)_webView:(WKWebView *)webView adjustedDataInteractionItemProvidersForItemProvider:(NSItemProvider *)itemProvider representingObjects:(NSArray *)representingObjects additionalData:(NSDictionary *)additionalData
{
    return self.convertItemProvidersBlock ? self.convertItemProvidersBlock(itemProvider, representingObjects, additionalData) : @[ itemProvider ];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)_webView:(WKWebView *)webView showCustomSheetForElement:(_WKActivatedElementInfo *)element
IGNORE_WARNINGS_END
{
    if (!self.showCustomActionSheetBlock)
        return NO;

    dispatch_async(dispatch_get_main_queue(), [strongSelf = retainPtr(self)] {
        [[strongSelf->_webView dragInteractionDelegate] dragInteraction:[strongSelf->_webView dragInteraction] sessionWillBegin:strongSelf->_dragSession.get()];
        strongSelf->_phase = DragAndDropPhaseBegan;
        [strongSelf _scheduleAdvanceProgress];
    });

    return self.showCustomActionSheetBlock(element);
}

- (NSArray<UIDragItem *> *)_webView:(WKWebView *)webView willPerformDropWithSession:(id <UIDropSession>)session
{
    return self.overridePerformDropBlock ? self.overridePerformDropBlock(session) : session.items;
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_insertedAttachments addObject:attachment];
}

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment
{
    [_removedAttachments addObject:attachment];
}

- (WKDragDestinationAction)_webView:(WKWebView *)webView dragDestinationActionMaskForDraggingInfo:(id)draggingInfo
{
    return self.dragDestinationAction;
}

#pragma mark - _WKInputDelegate

- (BOOL)_webView:(WKWebView *)webView focusShouldStartInputSession:(id <_WKFocusedElementInfo>)info
{
    return _allowsFocusToStartInputSession;
}

- (void)_webView:(WKWebView *)webView didStartInputSession:(id <_WKFormInputSession>)inputSession
{
    _hasStartedInputSession = true;
}

@end

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

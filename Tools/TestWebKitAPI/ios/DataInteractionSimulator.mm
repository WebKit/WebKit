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

#include "config.h"
#include "DataInteractionSimulator.h"

#if ENABLE(DATA_INTERACTION)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import <UIKit/UIDragInteraction.h>
#import <UIKit/UIDragItem.h>
#import <UIKit/UIDragSession.h>
#import <UIKit/UIDragging.h>
#import <UIKit/UIItemProvider_Private.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFocusedElementInfo.h>
#import <WebKit/_WKFormInputSession.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK(UIKit, UIApplicationInstantiateSingleton, void, (Class singletonClass), (singletonClass))

using namespace TestWebKitAPI;

@interface MockDragDropSession : NSObject <UIDragDropSession> {
@private
    RetainPtr<NSArray> _mockItems;
    RetainPtr<UIWindow> _window;
}
@property (nonatomic) CGPoint mockLocationInWindow;
@property (nonatomic) BOOL allowMove;
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

- (BOOL)canLoadObjectsOfClass:(Class<UIItemProviderReading>)aClass
{
    for (UIDragItem *item in self.items) {
        if ([item.itemProvider canLoadObjectOfClass:aClass])
            return YES;
    }
    return NO;
}

- (BOOL)canLoadObjectsOfClasses:(NSArray<Class<UIItemProviderReading>> *)classes
{
    for (Class<UIItemProviderReading> aClass in classes) {
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

- (CGPoint)locationInView:(UIView *)view
{
    return [_window convertPoint:_mockLocationInWindow toView:view];
}

@end

NSString * const DataInteractionEnterEventName = @"dragenter";
NSString * const DataInteractionOverEventName = @"dragover";
NSString * const DataInteractionPerformOperationEventName = @"drop";
NSString * const DataInteractionLeaveEventName = @"dragleave";
NSString * const DataInteractionStartEventName = @"dragstart";

@interface MockDataOperationSession : MockDragDropSession <UIDropSession>
@property (nonatomic, strong) id localContext;
@end

@implementation MockDataOperationSession

- (instancetype)initWithProviders:(NSArray<UIItemProvider *> *)providers location:(CGPoint)locationInWindow window:(UIWindow *)window allowMove:(BOOL)allowMove
{
    auto items = adoptNS([[NSMutableArray alloc] init]);
    for (UIItemProvider *itemProvider in providers)
        [items addObject:[[[UIDragItem alloc] initWithItemProvider:itemProvider] autorelease]];

    return [super initWithItems:items.get() location:locationInWindow window:window allowMove:allowMove];
}

- (UIDraggingSession *)session
{
    return nil;
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

- (BOOL)canCreateItemsOfClass:(Class<UIItemProviderReading>)aClass
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

@interface MockDataInteractionSession : MockDragDropSession <UIDragSession>
@property (nonatomic, strong) id localContext;
@property (nonatomic, strong) id context;
@end

@implementation MockDataInteractionSession

- (instancetype)initWithWindow:(UIWindow *)window allowMove:(BOOL)allowMove
{
    return [super initWithItems:@[ ] location:CGPointZero window:window allowMove:allowMove];
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

@end

static double progressIncrementStep = 0.033;
static double progressTimeStep = 0.016;
static NSString *TestWebKitAPISimulateCancelAllTouchesNotificationName = @"TestWebKitAPISimulateCancelAllTouchesNotificationName";

static NSArray *dataInteractionEventNames()
{
    static NSArray *eventNames = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^() {
        eventNames = @[ DataInteractionEnterEventName, DataInteractionOverEventName, DataInteractionPerformOperationEventName, DataInteractionLeaveEventName, DataInteractionStartEventName ];
    });
    return eventNames;
}

@interface DataInteractionSimulatorApplication : UIApplication
@end

@implementation DataInteractionSimulatorApplication
- (void)_cancelAllTouches
{
    [[NSNotificationCenter defaultCenter] postNotificationName:TestWebKitAPISimulateCancelAllTouchesNotificationName object:nil];
}
@end

@implementation DataInteractionSimulator

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (self = [super init]) {
        _webView = webView;
        _shouldEnsureUIApplication = NO;
        _shouldAllowMoveOperation = YES;
        _isDoneWaitingForInputSession = true;
        [_webView setUIDelegate:self];
        [_webView _setInputDelegate:self];
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
    _phase = DataInteractionBeginning;
    _currentProgress = 0;
    _isDoneWithCurrentRun = false;
    _observedEventNames = adoptNS([[NSMutableArray alloc] init]);
    _finalSelectionRects = @[ ];
    _dataInteractionSession = nil;
    _dataOperationSession = nil;
    _shouldPerformOperation = NO;
    _lastKnownDragCaretRect = CGRectZero;
}

- (NSArray *)observedEventNames
{
    return _observedEventNames.get();
}

- (void)simulateAllTouchesCanceled:(NSNotification *)notification
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_advanceProgress) object:nil];
    _phase = DataInteractionCancelled;
    _currentProgress = 1;
    _isDoneWithCurrentRun = true;
    if (_dataInteractionSession)
        [_webView _simulateDataInteractionSessionDidEnd:_dataInteractionSession.get()];
}

- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self selector:@selector(simulateAllTouchesCanceled:) name:TestWebKitAPISimulateCancelAllTouchesNotificationName object:nil];

    if (_shouldEnsureUIApplication)
        UIApplicationInstantiateSingleton([DataInteractionSimulatorApplication class]);

    [self _resetSimulatedState];

    RetainPtr<DataInteractionSimulator> strongSelf = self;
    for (NSString *eventName in dataInteractionEventNames()) {
        DataInteractionSimulator *weakSelf = strongSelf.get();
        [weakSelf->_webView performAfterReceivingMessage:eventName action:^() {
            [weakSelf->_observedEventNames addObject:eventName];
        }];
    }

    _startLocation = startLocation;
    _endLocation = endLocation;

    if (self.externalItemProviders.count) {
        _dataOperationSession = adoptNS([[MockDataOperationSession alloc] initWithProviders:self.externalItemProviders location:_startLocation window:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        _phase = DataInteractionBegan;
        [self _advanceProgress];
    } else {
        _dataInteractionSession = adoptNS([[MockDataInteractionSession alloc] initWithWindow:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        [_dataInteractionSession setMockLocationInWindow:_startLocation];
        [_webView _simulatePrepareForDataInteractionSession:_dataInteractionSession.get() completion:^() {
            DataInteractionSimulator *weakSelf = strongSelf.get();
            if (weakSelf->_phase == DataInteractionCancelled)
                return;

            weakSelf->_phase = DataInteractionBeginning;
            [weakSelf _advanceProgress];
        }];
    }

    Util::run(&_isDoneWithCurrentRun);
    [_webView clearMessageHandlers:dataInteractionEventNames()];
    _finalSelectionRects = [_webView selectionRectsAfterPresentationUpdate];

    [defaultCenter removeObserver:self];
}

- (NSArray *)finalSelectionRects
{
    return _finalSelectionRects.get();
}

- (void)_concludeDataInteractionAndPerformOperationIfNecessary
{
    _lastKnownDragCaretRect = [_webView _dragCaretRect];
    if (_shouldPerformOperation) {
        [_webView _simulateDataInteractionPerformOperation:_dataOperationSession.get()];
        _phase = DataInteractionPerforming;
    } else {
        _isDoneWithCurrentRun = YES;
        _phase = DataInteractionCancelled;
    }

    [_webView _simulateDataInteractionEnded:_dataOperationSession.get()];

    if (_dataInteractionSession)
        [_webView _simulateDataInteractionSessionDidEnd:_dataInteractionSession.get()];
}

- (void)_advanceProgress
{
    _lastKnownDragCaretRect = [_webView _dragCaretRect];
    _currentProgress += progressIncrementStep;
    CGPoint locationInWindow = self._currentLocation;
    [_dataInteractionSession setMockLocationInWindow:locationInWindow];
    [_dataOperationSession setMockLocationInWindow:locationInWindow];

    if (_currentProgress >= 1) {
        _currentProgress = 1;
        [self _concludeDataInteractionAndPerformOperationIfNecessary];
        return;
    }

    switch (_phase) {
    case DataInteractionBeginning: {
        NSMutableArray<UIItemProvider *> *itemProviders = [NSMutableArray array];
        NSArray *items = [_webView _simulatedItemsForSession:_dataInteractionSession.get()];
        if (!items.count) {
            _phase = DataInteractionCancelled;
            _currentProgress = 1;
            _isDoneWithCurrentRun = true;
            return;
        }

        for (UIDragItem *item in items)
            [itemProviders addObject:item.itemProvider];

        _dataOperationSession = adoptNS([[MockDataOperationSession alloc] initWithProviders:itemProviders location:self._currentLocation window:[_webView window] allowMove:self.shouldAllowMoveOperation]);
        [_dataInteractionSession setItems:items];
        _sourceItemProviders = itemProviders;
        if (self.showCustomActionSheetBlock) {
            // Defer progress until the custom action sheet is dismissed.
            auto startLocationInView = [[_webView window] convertPoint:_startLocation toView:_webView.get()];
            [_webView _simulateLongPressActionAtLocation:startLocationInView];
            return;
        }

        [_webView _simulateWillBeginDataInteractionWithSession:_dataInteractionSession.get()];

        RetainPtr<WKWebView> retainedWebView = _webView;
        dispatch_async(dispatch_get_main_queue(), ^() {
            [retainedWebView resignFirstResponder];
        });

        _phase = DataInteractionBegan;
        break;
    }
    case DataInteractionBegan:
        [_webView _simulateDataInteractionEntered:_dataOperationSession.get()];
        _phase = DataInteractionEntered;
        break;
    case DataInteractionEntered: {
        auto operation = static_cast<UIDropOperation>([_webView _simulateDataInteractionUpdated:_dataOperationSession.get()]);
        _shouldPerformOperation = operation == UIDropOperationCopy || ([_dataOperationSession allowsMoveOperation] && operation != UIDropOperationCancel);
        break;
    }
    default:
        break;
    }

    [self _scheduleAdvanceProgress];
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

- (DataInteractionPhase)phase
{
    return _phase;
}

- (CGRect)lastKnownDragCaretRect
{
    return _lastKnownDragCaretRect;
}

- (void)waitForInputSession
{
    _isDoneWaitingForInputSession = false;

    // Waiting for an input session implies that we should allow input sessions to begin.
    self.allowsFocusToStartInputSession = YES;

    Util::run(&_isDoneWaitingForInputSession);
}

#pragma mark - WKUIDelegatePrivate

- (void)_webView:(WKWebView *)webView dataInteractionOperationWasHandled:(BOOL)handled forSession:(id)session itemProviders:(NSArray<UIItemProvider *> *)itemProviders
{
    _isDoneWithCurrentRun = true;

    if (self.dataInteractionOperationCompletionBlock)
        self.dataInteractionOperationCompletionBlock(handled, itemProviders);
}

- (NSUInteger)_webView:(WKWebView *)webView willUpdateDataInteractionOperationToOperation:(NSUInteger)operation forSession:(id)session
{
    return self.overrideDataInteractionOperationBlock ? self.overrideDataInteractionOperationBlock(operation, session) : operation;
}

- (NSArray *)_webView:(WKWebView *)webView adjustedDataInteractionItemProvidersForItemProvider:(UIItemProvider *)itemProvider representingObjects:(NSArray *)representingObjects additionalData:(NSDictionary *)additionalData
{
    return self.convertItemProvidersBlock ? self.convertItemProvidersBlock(itemProvider, representingObjects, additionalData) : @[ itemProvider ];
}

- (BOOL)_webView:(WKWebView *)webView showCustomSheetForElement:(_WKActivatedElementInfo *)element
{
    if (!self.showCustomActionSheetBlock)
        return NO;

    RetainPtr<DataInteractionSimulator> strongSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^() {
        DataInteractionSimulator *weakSelf = strongSelf.get();
        [weakSelf->_webView _simulateWillBeginDataInteractionWithSession:weakSelf->_dataInteractionSession.get()];
        weakSelf->_phase = DataInteractionBegan;
        [weakSelf _scheduleAdvanceProgress];
    });

    return self.showCustomActionSheetBlock(element);
}

- (NSArray<UIDragItem *> *)_webView:(WKWebView *)webView willPerformDropWithSession:(id <UIDropSession>)session
{
    return self.overridePerformDropBlock ? self.overridePerformDropBlock(session) : session.items;
}

#pragma mark - _WKInputDelegate

- (BOOL)_webView:(WKWebView *)webView focusShouldStartInputSession:(id <_WKFocusedElementInfo>)info
{
    return _allowsFocusToStartInputSession;
}

- (void)_webView:(WKWebView *)webView didStartInputSession:(id <_WKFormInputSession>)inputSession
{
    _isDoneWaitingForInputSession = true;
}

@end

#endif // ENABLE(DATA_INTERACTION)

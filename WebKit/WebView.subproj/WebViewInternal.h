// This header contains WebView declarations that can be used anywhere in the Web Kit, but are neither SPI nor API.

#import <WebKit/WebViewPrivate.h>

@class DOMCSSStyleDeclaration;
@class WebBackForwardList;

@protocol WebDocumentDragging;

@interface WebViewPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    
    id UIDelegate;
    id UIDelegateForwarder;
    id resourceProgressDelegate;
    id resourceProgressDelegateForwarder;
    id downloadDelegate;
    id policyDelegate;
    id policyDelegateForwarder;
    id frameLoadDelegate;
    id frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    id editingDelegate;
    id editingDelegateForwarder;
    
    WebBackForwardList *backForwardList;
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgent;
    BOOL userAgentOverridden;
    
    BOOL defersCallbacks;

    NSString *setName;

    WebPreferences *preferences;
    WebCoreSettings *settings;
        
    BOOL lastElementWasNonNil;

    NSWindow *hostWindow;

    int programmaticFocusCount;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;

    long long totalPageAndResourceBytesToLoad;
    long long totalBytesReceived;
    double progressValue;
    double lastNotifiedProgressValue;
    double lastNotifiedProgressTime;
    double progressNotificationInterval;
    double progressNotificationTimeInterval;
    BOOL finalProgressChangedSent;
    WebFrame *orginatingProgressFrame;
    
    int numProgressTrackedFrames;
    NSMutableDictionary *progressItems;
    
    void *observationInfo;
    
    BOOL drawsBackground;
    BOOL editable;
    BOOL initiatedDrag;
        
    NSString *mediaStyle;
    
    NSView <WebDocumentDragging> *draggingDocumentView;
    unsigned int dragDestinationActionMask;
    WebBridge *dragCaretBridge;
    
    BOOL hasSpellCheckerDocumentTag;
    int spellCheckerDocumentTag;

    BOOL continuousSpellCheckingEnabled;
    BOOL continuousGrammarCheckingEnabled;
    BOOL smartInsertDeleteEnabled;
    
    BOOL dashboardBehaviorAlwaysSendMouseEventsToAllWindows;
    BOOL dashboardBehaviorAlwaysSendActiveNullEventsToPlugIns;
    BOOL dashboardBehaviorAlwaysAcceptsFirstMouse;
    
    BOOL shouldUseFontSmoothing;
}
@end

@interface WebView (WebInternal)
- (WebFrame *)_frameForCurrentSelection;
- (WebBridge *)_bridgeForCurrentSelection;
- (BOOL)_isLoading;

- (WebFrameView *)_frameViewAtWindowPoint:(NSPoint)point;
- (WebBridge *)_bridgeAtPoint:(NSPoint)point;
@end;

@interface WebView (WebViewEditingExtras)
- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event;
- (BOOL)_shouldBeginEditingInDOMRange:(DOMRange *)range;
- (BOOL)_shouldEndEditingInDOMRange:(DOMRange *)range;
- (BOOL)_canPaste;
@end

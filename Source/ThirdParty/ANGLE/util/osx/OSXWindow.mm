//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// OSXWindow.mm: Implementation of OSWindow for OSX

#include "util/osx/OSXWindow.h"

#include <set>
// Include Carbon to use the keycode names in Carbon's Event.h
#include <Carbon/Carbon.h>

#include "anglebase/no_destructor.h"
#include "common/debug.h"

// On OSX 10.12 a number of AppKit interfaces have been renamed for consistency, and the previous
// symbols tagged as deprecated. However we can't simply use the new symbols as it would break
// compilation on our automated testing that doesn't use OSX 10.12 yet. So we just ignore the
// warnings.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// Some events such as "ShouldTerminate" are sent to the whole application so we keep a list of
// all the windows in order to forward the event to each of them. However this and calling pushEvent
// in ApplicationDelegate is inherently unsafe in a multithreaded environment.
static std::set<OSXWindow *> &AllWindows()
{
    static angle::base::NoDestructor<std::set<OSXWindow *>> allWindows;
    return *allWindows;
}

@interface Application : NSApplication
@end

@implementation Application
- (void)sendEvent:(NSEvent *)nsEvent
{
    if ([nsEvent type] == NSApplicationDefined)
    {
        for (auto window : AllWindows())
        {
            if ([window->getNSWindow() windowNumber] == [nsEvent windowNumber])
            {
                Event event;
                event.Type = Event::EVENT_TEST;
                window->pushEvent(event);
            }
        }
    }
    [super sendEvent:nsEvent];
}

// Override internal method to try to diagnose unexpected crashes in Core Animation.
// anglebug.com/42265067
// See also:
//   https://github.com/microsoft/appcenter-sdk-apple/issues/1944
//   https://stackoverflow.com/questions/220159/how-do-you-print-out-a-stack-trace-to-the-console-log-in-cocoa
- (void)_crashOnException:(NSException *)exception
{
    NSLog(@"*** OSXWindow aborting on exception:  <%@> %@", [exception name], [exception reason]);
    NSLog(@"%@", [exception callStackSymbols]);
    abort();
}
@end

// The Delegate receiving application-wide events.
@interface ApplicationDelegate : NSObject
@end

@implementation ApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    Event event;
    event.Type = Event::EVENT_CLOSED;
    for (auto window : AllWindows())
    {
        window->pushEvent(event);
    }
    return NSTerminateCancel;
}
@end
static ApplicationDelegate *gApplicationDelegate = nil;

static bool InitializeAppKit()
{
    if (NSApp != nil)
    {
        return true;
    }

    // Initialize the global variable "NSApp"
    [Application sharedApplication];

    // Make us appear in the dock
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // Register our global event handler
    gApplicationDelegate = [[ApplicationDelegate alloc] init];
    if (gApplicationDelegate == nil)
    {
        return false;
    }
    [NSApp setDelegate:static_cast<id>(gApplicationDelegate)];

    // Set our status to "started" so we are not bouncing in the doc and can activate
    [NSApp finishLaunching];
    return true;
}

// NS's and CG's coordinate systems start at the bottom left, while OSWindow's coordinate
// system starts at the top left. This function converts the Y coordinate accordingly.
static float YCoordToFromCG(float y)
{
    float screenHeight = CGDisplayBounds(CGMainDisplayID()).size.height;
    return screenHeight - y;
}

// Delegate for window-wide events, note that the protocol doesn't contain anything input related.
@implementation WindowDelegate
- (id)initWithWindow:(OSXWindow *)window
{
    self = [super init];
    if (self != nil)
    {
        mWindow = window;
    }
    return self;
}

- (void)onOSXWindowDeleted
{
    mWindow = nil;
}

- (BOOL)windowShouldClose:(id)sender
{
    Event event;
    event.Type = Event::EVENT_CLOSED;
    mWindow->pushEvent(event);
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification
{
    NSSize windowSize = [[mWindow->getNSWindow() contentView] frame].size;
    Event event;
    event.Type        = Event::EVENT_RESIZED;
    event.Size.Width  = (int)windowSize.width;
    event.Size.Height = (int)windowSize.height;
    mWindow->pushEvent(event);
}

- (void)windowDidMove:(NSNotification *)notification
{
    NSRect screenspace = [mWindow->getNSWindow() frame];
    Event event;
    event.Type   = Event::EVENT_MOVED;
    event.Move.X = (int)screenspace.origin.x;
    event.Move.Y = (int)YCoordToFromCG(screenspace.origin.y + screenspace.size.height);
    mWindow->pushEvent(event);
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    Event event;
    event.Type = Event::EVENT_GAINED_FOCUS;
    mWindow->pushEvent(event);
    [self retain];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if (mWindow != nil)
    {
        Event event;
        event.Type = Event::EVENT_LOST_FOCUS;
        mWindow->pushEvent(event);
    }
    [self release];
}
@end

static angle::KeyType NSCodeToKey(int keyCode)
{
    // Missing KEY_PAUSE
    switch (keyCode)
    {
        case kVK_Shift:
            return angle::KeyType::LSHIFT;
        case kVK_RightShift:
            return angle::KeyType::RSHIFT;
        case kVK_Option:
            return angle::KeyType::LALT;
        case kVK_RightOption:
            return angle::KeyType::RALT;
        case kVK_Control:
            return angle::KeyType::LCONTROL;
        case kVK_RightControl:
            return angle::KeyType::RCONTROL;
        case kVK_Command:
            return angle::KeyType::LSYSTEM;
        // Right System doesn't have a name, but shows up as 0x36.
        case 0x36:
            return angle::KeyType::RSYSTEM;
        case kVK_Function:
            return angle::KeyType::MENU;

        case kVK_ANSI_Semicolon:
            return angle::KeyType::SEMICOLON;
        case kVK_ANSI_Slash:
            return angle::KeyType::SLASH;
        case kVK_ANSI_Equal:
            return angle::KeyType::EQUAL;
        case kVK_ANSI_Minus:
            return angle::KeyType::DASH;
        case kVK_ANSI_LeftBracket:
            return angle::KeyType::LBRACKET;
        case kVK_ANSI_RightBracket:
            return angle::KeyType::RBRACKET;
        case kVK_ANSI_Comma:
            return angle::KeyType::COMMA;
        case kVK_ANSI_Period:
            return angle::KeyType::PERIOD;
        case kVK_ANSI_Backslash:
            return angle::KeyType::BACKSLASH;
        case kVK_ANSI_Grave:
            return angle::KeyType::TILDE;
        case kVK_Escape:
            return angle::KeyType::ESCAPE;
        case kVK_Space:
            return angle::KeyType::SPACE;
        case kVK_Return:
            return angle::KeyType::RETURN;
        case kVK_Delete:
            return angle::KeyType::BACK;
        case kVK_Tab:
            return angle::KeyType::TAB;
        case kVK_PageUp:
            return angle::KeyType::PAGEUP;
        case kVK_PageDown:
            return angle::KeyType::PAGEDOWN;
        case kVK_End:
            return angle::KeyType::END;
        case kVK_Home:
            return angle::KeyType::HOME;
        case kVK_Help:
            return angle::KeyType::INSERT;
        case kVK_ForwardDelete:
            return angle::KeyType::DEL;
        case kVK_ANSI_KeypadPlus:
            return angle::KeyType::ADD;
        case kVK_ANSI_KeypadMinus:
            return angle::KeyType::SUBTRACT;
        case kVK_ANSI_KeypadMultiply:
            return angle::KeyType::MULTIPLY;
        case kVK_ANSI_KeypadDivide:
            return angle::KeyType::DIVIDE;

        case kVK_F1:
            return angle::KeyType::F1;
        case kVK_F2:
            return angle::KeyType::F2;
        case kVK_F3:
            return angle::KeyType::F3;
        case kVK_F4:
            return angle::KeyType::F4;
        case kVK_F5:
            return angle::KeyType::F5;
        case kVK_F6:
            return angle::KeyType::F6;
        case kVK_F7:
            return angle::KeyType::F7;
        case kVK_F8:
            return angle::KeyType::F8;
        case kVK_F9:
            return angle::KeyType::F9;
        case kVK_F10:
            return angle::KeyType::F10;
        case kVK_F11:
            return angle::KeyType::F11;
        case kVK_F12:
            return angle::KeyType::F12;
        case kVK_F13:
            return angle::KeyType::F13;
        case kVK_F14:
            return angle::KeyType::F14;
        case kVK_F15:
            return angle::KeyType::F15;

        case kVK_LeftArrow:
            return angle::KeyType::LEFT;
        case kVK_RightArrow:
            return angle::KeyType::RIGHT;
        case kVK_DownArrow:
            return angle::KeyType::DOWN;
        case kVK_UpArrow:
            return angle::KeyType::UP;

        case kVK_ANSI_Keypad0:
            return angle::KeyType::NUMPAD0;
        case kVK_ANSI_Keypad1:
            return angle::KeyType::NUMPAD1;
        case kVK_ANSI_Keypad2:
            return angle::KeyType::NUMPAD2;
        case kVK_ANSI_Keypad3:
            return angle::KeyType::NUMPAD3;
        case kVK_ANSI_Keypad4:
            return angle::KeyType::NUMPAD4;
        case kVK_ANSI_Keypad5:
            return angle::KeyType::NUMPAD5;
        case kVK_ANSI_Keypad6:
            return angle::KeyType::NUMPAD6;
        case kVK_ANSI_Keypad7:
            return angle::KeyType::NUMPAD7;
        case kVK_ANSI_Keypad8:
            return angle::KeyType::NUMPAD8;
        case kVK_ANSI_Keypad9:
            return angle::KeyType::NUMPAD9;

        case kVK_ANSI_A:
            return angle::KeyType::A;
        case kVK_ANSI_B:
            return angle::KeyType::B;
        case kVK_ANSI_C:
            return angle::KeyType::C;
        case kVK_ANSI_D:
            return angle::KeyType::D;
        case kVK_ANSI_E:
            return angle::KeyType::E;
        case kVK_ANSI_F:
            return angle::KeyType::F;
        case kVK_ANSI_G:
            return angle::KeyType::G;
        case kVK_ANSI_H:
            return angle::KeyType::H;
        case kVK_ANSI_I:
            return angle::KeyType::I;
        case kVK_ANSI_J:
            return angle::KeyType::J;
        case kVK_ANSI_K:
            return angle::KeyType::K;
        case kVK_ANSI_L:
            return angle::KeyType::L;
        case kVK_ANSI_M:
            return angle::KeyType::M;
        case kVK_ANSI_N:
            return angle::KeyType::N;
        case kVK_ANSI_O:
            return angle::KeyType::O;
        case kVK_ANSI_P:
            return angle::KeyType::P;
        case kVK_ANSI_Q:
            return angle::KeyType::Q;
        case kVK_ANSI_R:
            return angle::KeyType::R;
        case kVK_ANSI_S:
            return angle::KeyType::S;
        case kVK_ANSI_T:
            return angle::KeyType::T;
        case kVK_ANSI_U:
            return angle::KeyType::U;
        case kVK_ANSI_V:
            return angle::KeyType::V;
        case kVK_ANSI_W:
            return angle::KeyType::W;
        case kVK_ANSI_X:
            return angle::KeyType::X;
        case kVK_ANSI_Y:
            return angle::KeyType::Y;
        case kVK_ANSI_Z:
            return angle::KeyType::Z;

        case kVK_ANSI_1:
            return angle::KeyType::NUM1;
        case kVK_ANSI_2:
            return angle::KeyType::NUM2;
        case kVK_ANSI_3:
            return angle::KeyType::NUM3;
        case kVK_ANSI_4:
            return angle::KeyType::NUM4;
        case kVK_ANSI_5:
            return angle::KeyType::NUM5;
        case kVK_ANSI_6:
            return angle::KeyType::NUM6;
        case kVK_ANSI_7:
            return angle::KeyType::NUM7;
        case kVK_ANSI_8:
            return angle::KeyType::NUM8;
        case kVK_ANSI_9:
            return angle::KeyType::NUM9;
        case kVK_ANSI_0:
            return angle::KeyType::NUM0;
    }

    return angle::KeyType(0);
}

static void AddNSKeyStateToEvent(Event *event, NSEventModifierFlags state)
{
    event->Key.Shift   = state & NSShiftKeyMask;
    event->Key.Control = state & NSControlKeyMask;
    event->Key.Alt     = state & NSAlternateKeyMask;
    event->Key.System  = state & NSCommandKeyMask;
}

static angle::MouseButtonType TranslateMouseButton(NSInteger button)
{
    switch (button)
    {
        case 2:
            return angle::MouseButtonType::MIDDLE;
        case 3:
            return angle::MouseButtonType::BUTTON4;
        case 4:
            return angle::MouseButtonType::BUTTON5;
        default:
            return angle::MouseButtonType::UNKNOWN;
    }
}

// Delegate for NSView events, mostly the input events
@implementation ContentView
- (id)initWithWindow:(OSXWindow *)window
{
    self = [super init];
    if (self != nil)
    {
        mWindow          = window;
        mTrackingArea    = nil;
        mCurrentModifier = 0;
        [self updateTrackingAreas];
    }
    return self;
}

- (void)dealloc
{
    [mTrackingArea release];
    [super dealloc];
}

- (void)updateTrackingAreas
{
    if (mTrackingArea != nil)
    {
        [self removeTrackingArea:mTrackingArea];
        [mTrackingArea release];
        mTrackingArea = nil;
    }

    NSRect bounds               = [self bounds];
    NSTrackingAreaOptions flags = NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow |
                                  NSTrackingCursorUpdate | NSTrackingInVisibleRect |
                                  NSTrackingAssumeInside;
    mTrackingArea = [[NSTrackingArea alloc] initWithRect:bounds
                                                 options:flags
                                                   owner:self
                                                userInfo:nil];

    [self addTrackingArea:mTrackingArea];
    [super updateTrackingAreas];
}

// Helps with performance
- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

// Handle mouse events from the NSResponder protocol
- (float)translateMouseY:(float)y
{
    return [self frame].size.height - y;
}

- (void)addButtonEvent:(NSEvent *)nsEvent
                  type:(Event::EventType)eventType
                button:(angle::MouseButtonType)button
{
    Event event;
    event.Type               = eventType;
    event.MouseButton.Button = button;
    event.MouseButton.X      = (int)[nsEvent locationInWindow].x;
    event.MouseButton.Y      = (int)[self translateMouseY:[nsEvent locationInWindow].y];
    mWindow->pushEvent(event);
}

- (void)mouseDown:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_PRESSED
                  button:angle::MouseButtonType::LEFT];
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_RELEASED
                  button:angle::MouseButtonType::LEFT];
}

- (void)mouseMoved:(NSEvent *)nsEvent
{
    Event event;
    event.Type        = Event::EVENT_MOUSE_MOVED;
    event.MouseMove.X = (int)[nsEvent locationInWindow].x;
    event.MouseMove.Y = (int)[self translateMouseY:[nsEvent locationInWindow].y];
    mWindow->pushEvent(event);
}

- (void)mouseEntered:(NSEvent *)nsEvent
{
    Event event;
    event.Type = Event::EVENT_MOUSE_ENTERED;
    mWindow->pushEvent(event);
}

- (void)mouseExited:(NSEvent *)nsEvent
{
    Event event;
    event.Type = Event::EVENT_MOUSE_LEFT;
    mWindow->pushEvent(event);
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_PRESSED
                  button:angle::MouseButtonType::RIGHT];
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_RELEASED
                  button:angle::MouseButtonType::RIGHT];
}

- (void)otherMouseDown:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_PRESSED
                  button:TranslateMouseButton([event buttonNumber])];
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    [self addButtonEvent:event
                    type:Event::EVENT_MOUSE_BUTTON_RELEASED
                  button:TranslateMouseButton([event buttonNumber])];
}

- (void)scrollWheel:(NSEvent *)nsEvent
{
    if (static_cast<int>([nsEvent deltaY]) == 0)
    {
        return;
    }

    Event event;
    event.Type             = Event::EVENT_MOUSE_WHEEL_MOVED;
    event.MouseWheel.Delta = (int)[nsEvent deltaY];
    mWindow->pushEvent(event);
}

// Handle key events from the NSResponder protocol
- (void)keyDown:(NSEvent *)nsEvent
{
    // TODO(cwallez) also send text events
    Event event;
    event.Type     = Event::EVENT_KEY_PRESSED;
    event.Key.Code = NSCodeToKey([nsEvent keyCode]);
    AddNSKeyStateToEvent(&event, [nsEvent modifierFlags]);
    mWindow->pushEvent(event);
}

- (void)keyUp:(NSEvent *)nsEvent
{
    Event event;
    event.Type     = Event::EVENT_KEY_RELEASED;
    event.Key.Code = NSCodeToKey([nsEvent keyCode]);
    AddNSKeyStateToEvent(&event, [nsEvent modifierFlags]);
    mWindow->pushEvent(event);
}

// Modifier keys do not trigger keyUp/Down events but only flagsChanged events.
- (void)flagsChanged:(NSEvent *)nsEvent
{
    Event event;

    // Guess if the key has been pressed or released with the change of modifiers
    // It currently doesn't work when modifiers are unchanged, such as when pressing
    // both shift keys. GLFW has a solution for this but it requires tracking the
    // state of the keys. Implementing this is still TODO(cwallez)
    int modifier = [nsEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
    if (modifier < mCurrentModifier)
    {
        event.Type = Event::EVENT_KEY_RELEASED;
    }
    else
    {
        event.Type = Event::EVENT_KEY_PRESSED;
    }
    mCurrentModifier = modifier;

    event.Key.Code = NSCodeToKey([nsEvent keyCode]);
    AddNSKeyStateToEvent(&event, [nsEvent modifierFlags]);
    mWindow->pushEvent(event);
}
@end

OSXWindow::OSXWindow() : mWindow(nil), mDelegate(nil), mView(nil) {}

OSXWindow::~OSXWindow()
{
    destroy();
}

bool OSXWindow::initializeImpl(const std::string &name, int width, int height)
{
    if (!InitializeAppKit())
    {
        return false;
    }

    unsigned int styleMask = NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask |
                             NSMiniaturizableWindowMask;
    mWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];

    if (mWindow == nil)
    {
        return false;
    }

    mDelegate = [[WindowDelegate alloc] initWithWindow:this];
    if (mDelegate == nil)
    {
        return false;
    }
    [mWindow setDelegate:static_cast<id>(mDelegate)];

    mView = [[ContentView alloc] initWithWindow:this];
    if (mView == nil)
    {
        return false;
    }
    [mView setWantsLayer:YES];

    // Disable scaling for this view. If scaling is enabled, the metal backend's
    // frame buffer's size will be this window's size multiplied by contentScale.
    // It will cause inconsistent testing & example apps' results.
    mView.layer.contentsScale = 1;

    [mWindow setContentView:mView];
    [mWindow setTitle:[NSString stringWithUTF8String:name.c_str()]];
    [mWindow setAcceptsMouseMovedEvents:YES];
    [mWindow center];

    [NSApp activateIgnoringOtherApps:YES];

    mX      = 0;
    mY      = 0;
    mWidth  = width;
    mHeight = height;

    AllWindows().insert(this);
    return true;
}

void OSXWindow::disableErrorMessageDialog() {}

void OSXWindow::destroy()
{
    AllWindows().erase(this);

    [mView release];
    mView = nil;
    [mDelegate onOSXWindowDeleted];
    [mDelegate release];
    mDelegate = nil;
    // NSWindow won't be completely released unless its content view is set to nil:
    [mWindow setContentView:nil];
    [mWindow release];
    mWindow = nil;
}

void OSXWindow::resetNativeWindow() {}

EGLNativeWindowType OSXWindow::getNativeWindow() const
{
    return [mView layer];
}

EGLNativeDisplayType OSXWindow::getNativeDisplay() const
{
    // TODO(cwallez): implement it once we have defined what EGLNativeDisplayType is
    return static_cast<EGLNativeDisplayType>(0);
}

void OSXWindow::messageLoop()
{
    @autoreleasepool
    {
        while (true)
        {
            // TODO(http://anglebug.com/42265067): @try/@catch is a workaround for
            // exceptions being thrown from Cocoa-internal function
            // NS_setFlushesWithDisplayLink starting in macOS 11.
            @try
            {
                NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                                    untilDate:[NSDate distantPast]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (event == nil)
                {
                    break;
                }

                if ([event type] == NSAppKitDefined)
                {
                    continue;
                }
                [NSApp sendEvent:event];
            }
            @catch (NSException *localException)
            {
                NSLog(@"*** OSXWindow discarding exception: <%@> %@", [localException name],
                      [localException reason]);
            }
        }
    }
}

void OSXWindow::setMousePosition(int x, int y)
{
    y = (int)([mWindow frame].size.height) - y - 1;
    NSPoint screenspace;

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7
    screenspace = [mWindow convertBaseToScreen:NSMakePoint(x, y)];
#else
    screenspace = [mWindow convertRectToScreen:NSMakeRect(x, y, 0, 0)].origin;
#endif
    CGWarpMouseCursorPosition(CGPointMake(screenspace.x, YCoordToFromCG(screenspace.y)));
}

bool OSXWindow::setOrientation(int width, int height)
{
    UNIMPLEMENTED();
    return false;
}

bool OSXWindow::setPosition(int x, int y)
{
    // Given CG and NS's coordinate system, the "Y" position of a window is the Y coordinate
    // of the bottom of the window.
    int newBottom    = (int)([mWindow frame].size.height) + y;
    NSRect emptyRect = NSMakeRect(x, YCoordToFromCG(newBottom), 0, 0);
    [mWindow setFrameOrigin:[mWindow frameRectForContentRect:emptyRect].origin];
    return true;
}

bool OSXWindow::resize(int width, int height)
{
    [mWindow setContentSize:NSMakeSize(width, height)];
    return true;
}

void OSXWindow::setVisible(bool isVisible)
{
    if (isVisible)
    {
        [mWindow makeKeyAndOrderFront:nil];
    }
    else
    {
        [mWindow orderOut:nil];
    }
}

void OSXWindow::signalTestEvent()
{
    @autoreleasepool
    {
        NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined
                                            location:NSMakePoint(0, 0)
                                       modifierFlags:0
                                           timestamp:0.0
                                        windowNumber:[mWindow windowNumber]
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0];
        [NSApp postEvent:event atStart:YES];
    }
}

NSWindow *OSXWindow::getNSWindow() const
{
    return mWindow;
}

// static
OSWindow *OSWindow::New()
{
    return new OSXWindow;
}

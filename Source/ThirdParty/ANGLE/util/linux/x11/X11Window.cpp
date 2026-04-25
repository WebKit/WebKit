//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// X11Window.cpp: Implementation of OSWindow for X11

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "util/linux/x11/X11Window.h"

#include "common/debug.h"
#include "util/Timer.h"
#include "util/test_utils.h"

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

namespace
{

Bool WaitForMapNotify(Display *dpy, XEvent *event, XPointer window)
{
    return event->type == MapNotify && event->xmap.window == reinterpret_cast<Window>(window);
}

static angle::KeyType X11CodeToKey(Display *display, unsigned int scancode)
{
    int temp;
    KeySym *keySymbols;
    keySymbols = XGetKeyboardMapping(display, scancode, 1, &temp);

    KeySym keySymbol = keySymbols[0];
    XFree(keySymbols);

    switch (keySymbol)
    {
        case XK_Shift_L:
            return angle::KeyType::LSHIFT;
        case XK_Shift_R:
            return angle::KeyType::RSHIFT;
        case XK_Alt_L:
            return angle::KeyType::LALT;
        case XK_Alt_R:
            return angle::KeyType::RALT;
        case XK_Control_L:
            return angle::KeyType::LCONTROL;
        case XK_Control_R:
            return angle::KeyType::RCONTROL;
        case XK_Super_L:
            return angle::KeyType::LSYSTEM;
        case XK_Super_R:
            return angle::KeyType::RSYSTEM;
        case XK_Menu:
            return angle::KeyType::MENU;

        case XK_semicolon:
            return angle::KeyType::SEMICOLON;
        case XK_slash:
            return angle::KeyType::SLASH;
        case XK_equal:
            return angle::KeyType::EQUAL;
        case XK_minus:
            return angle::KeyType::DASH;
        case XK_bracketleft:
            return angle::KeyType::LBRACKET;
        case XK_bracketright:
            return angle::KeyType::RBRACKET;
        case XK_comma:
            return angle::KeyType::COMMA;
        case XK_period:
            return angle::KeyType::PERIOD;
        case XK_backslash:
            return angle::KeyType::BACKSLASH;
        case XK_asciitilde:
            return angle::KeyType::TILDE;
        case XK_Escape:
            return angle::KeyType::ESCAPE;
        case XK_space:
            return angle::KeyType::SPACE;
        case XK_Return:
            return angle::KeyType::RETURN;
        case XK_BackSpace:
            return angle::KeyType::BACK;
        case XK_Tab:
            return angle::KeyType::TAB;
        case XK_Page_Up:
            return angle::KeyType::PAGEUP;
        case XK_Page_Down:
            return angle::KeyType::PAGEDOWN;
        case XK_End:
            return angle::KeyType::END;
        case XK_Home:
            return angle::KeyType::HOME;
        case XK_Insert:
            return angle::KeyType::INSERT;
        case XK_Delete:
            return angle::KeyType::DEL;
        case XK_KP_Add:
            return angle::KeyType::ADD;
        case XK_KP_Subtract:
            return angle::KeyType::SUBTRACT;
        case XK_KP_Multiply:
            return angle::KeyType::MULTIPLY;
        case XK_KP_Divide:
            return angle::KeyType::DIVIDE;
        case XK_Pause:
            return angle::KeyType::PAUSE;

        case XK_F1:
            return angle::KeyType::F1;
        case XK_F2:
            return angle::KeyType::F2;
        case XK_F3:
            return angle::KeyType::F3;
        case XK_F4:
            return angle::KeyType::F4;
        case XK_F5:
            return angle::KeyType::F5;
        case XK_F6:
            return angle::KeyType::F6;
        case XK_F7:
            return angle::KeyType::F7;
        case XK_F8:
            return angle::KeyType::F8;
        case XK_F9:
            return angle::KeyType::F9;
        case XK_F10:
            return angle::KeyType::F10;
        case XK_F11:
            return angle::KeyType::F11;
        case XK_F12:
            return angle::KeyType::F12;
        case XK_F13:
            return angle::KeyType::F13;
        case XK_F14:
            return angle::KeyType::F14;
        case XK_F15:
            return angle::KeyType::F15;

        case XK_Left:
            return angle::KeyType::LEFT;
        case XK_Right:
            return angle::KeyType::RIGHT;
        case XK_Down:
            return angle::KeyType::DOWN;
        case XK_Up:
            return angle::KeyType::UP;

        case XK_KP_Insert:
            return angle::KeyType::NUMPAD0;
        case XK_KP_End:
            return angle::KeyType::NUMPAD1;
        case XK_KP_Down:
            return angle::KeyType::NUMPAD2;
        case XK_KP_Page_Down:
            return angle::KeyType::NUMPAD3;
        case XK_KP_Left:
            return angle::KeyType::NUMPAD4;
        case XK_KP_5:
            return angle::KeyType::NUMPAD5;
        case XK_KP_Right:
            return angle::KeyType::NUMPAD6;
        case XK_KP_Home:
            return angle::KeyType::NUMPAD7;
        case XK_KP_Up:
            return angle::KeyType::NUMPAD8;
        case XK_KP_Page_Up:
            return angle::KeyType::NUMPAD9;

        case XK_a:
            return angle::KeyType::A;
        case XK_b:
            return angle::KeyType::B;
        case XK_c:
            return angle::KeyType::C;
        case XK_d:
            return angle::KeyType::D;
        case XK_e:
            return angle::KeyType::E;
        case XK_f:
            return angle::KeyType::F;
        case XK_g:
            return angle::KeyType::G;
        case XK_h:
            return angle::KeyType::H;
        case XK_i:
            return angle::KeyType::I;
        case XK_j:
            return angle::KeyType::J;
        case XK_k:
            return angle::KeyType::K;
        case XK_l:
            return angle::KeyType::L;
        case XK_m:
            return angle::KeyType::M;
        case XK_n:
            return angle::KeyType::N;
        case XK_o:
            return angle::KeyType::O;
        case XK_p:
            return angle::KeyType::P;
        case XK_q:
            return angle::KeyType::Q;
        case XK_r:
            return angle::KeyType::R;
        case XK_s:
            return angle::KeyType::S;
        case XK_t:
            return angle::KeyType::T;
        case XK_u:
            return angle::KeyType::U;
        case XK_v:
            return angle::KeyType::V;
        case XK_w:
            return angle::KeyType::W;
        case XK_x:
            return angle::KeyType::X;
        case XK_y:
            return angle::KeyType::Y;
        case XK_z:
            return angle::KeyType::Z;

        case XK_1:
            return angle::KeyType::NUM1;
        case XK_2:
            return angle::KeyType::NUM2;
        case XK_3:
            return angle::KeyType::NUM3;
        case XK_4:
            return angle::KeyType::NUM4;
        case XK_5:
            return angle::KeyType::NUM5;
        case XK_6:
            return angle::KeyType::NUM6;
        case XK_7:
            return angle::KeyType::NUM7;
        case XK_8:
            return angle::KeyType::NUM8;
        case XK_9:
            return angle::KeyType::NUM9;
        case XK_0:
            return angle::KeyType::NUM0;
    }

    return angle::KeyType(0);
}

static void AddX11KeyStateToEvent(Event *event, unsigned int state)
{
    event->Key.Shift   = state & ShiftMask;
    event->Key.Control = state & ControlMask;
    event->Key.Alt     = state & Mod1Mask;
    event->Key.System  = state & Mod4Mask;
}

void setWindowSizeHints(Display *display, Window window, int width, int height)
{
    // Set PMinSize and PMaxSize on XSizeHints so windows larger than the screen do not get adjusted
    // to screen size
    XSizeHints *sizeHints = XAllocSizeHints();
    sizeHints->flags      = PMinSize | PMaxSize;
    sizeHints->min_width  = width;
    sizeHints->min_height = height;
    sizeHints->max_width  = width;
    sizeHints->max_height = height;

    XSetWMNormalHints(display, window, sizeHints);

    XFree(sizeHints);
}

}  // namespace

class ANGLE_UTIL_EXPORT X11Window : public OSWindow
{
  public:
    X11Window();
    X11Window(int visualId);
    ~X11Window() override;

    void disableErrorMessageDialog() override;
    void destroy() override;

    void resetNativeWindow() override;
    EGLNativeWindowType getNativeWindow() const override;
    void *getPlatformExtension() override;
    EGLNativeDisplayType getNativeDisplay() const override;

    void messageLoop() override;

    void setMousePosition(int x, int y) override;
    bool setOrientation(int width, int height) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;

    void signalTestEvent() override;

  private:
    bool initializeImpl(const std::string &name, int width, int height) override;
    void processEvent(const XEvent &event);

    Atom WM_DELETE_WINDOW;
    Atom WM_PROTOCOLS;
    Atom TEST_EVENT;

    Display *mDisplay;
    Window mWindow;
    int mRequestedVisualId;
    bool mVisible;
    bool mConfigured = false;
    bool mDestroyed  = false;
};

X11Window::X11Window()
    : WM_DELETE_WINDOW(None),
      WM_PROTOCOLS(None),
      TEST_EVENT(None),
      mDisplay(nullptr),
      mWindow(0),
      mRequestedVisualId(-1),
      mVisible(false)
{}

X11Window::X11Window(int visualId)
    : WM_DELETE_WINDOW(None),
      WM_PROTOCOLS(None),
      TEST_EVENT(None),
      mDisplay(nullptr),
      mWindow(0),
      mRequestedVisualId(visualId),
      mVisible(false)
{}

X11Window::~X11Window()
{
    destroy();
}

bool X11Window::initializeImpl(const std::string &name, int width, int height)
{
    destroy();

    mDisplay = XOpenDisplay(nullptr);
    if (!mDisplay)
    {
        return false;
    }

    {
        int screen  = DefaultScreen(mDisplay);
        Window root = RootWindow(mDisplay, screen);

        Visual *visual;
        if (mRequestedVisualId == -1)
        {
            visual = DefaultVisual(mDisplay, screen);
        }
        else
        {
            XVisualInfo visualTemplate;
            visualTemplate.visualid = mRequestedVisualId;

            int numVisuals = 0;
            XVisualInfo *visuals =
                XGetVisualInfo(mDisplay, VisualIDMask, &visualTemplate, &numVisuals);
            if (numVisuals <= 0)
            {
                return false;
            }
            ASSERT(numVisuals == 1);

            visual = visuals[0].visual;
            XFree(visuals);
        }

        int depth         = DefaultDepth(mDisplay, screen);
        Colormap colormap = XCreateColormap(mDisplay, root, visual, AllocNone);

        XSetWindowAttributes attributes;
        unsigned long attributeMask = CWBorderPixel | CWColormap | CWEventMask;

        attributes.event_mask = StructureNotifyMask | PointerMotionMask | ButtonPressMask |
                                ButtonReleaseMask | FocusChangeMask | EnterWindowMask |
                                LeaveWindowMask | KeyPressMask | KeyReleaseMask;
        attributes.border_pixel = 0;
        attributes.colormap     = colormap;

        mWindow = XCreateWindow(mDisplay, root, 0, 0, width, height, 0, depth, InputOutput, visual,
                                attributeMask, &attributes);
        XFreeColormap(mDisplay, colormap);
    }

    if (!mWindow)
    {
        destroy();
        return false;
    }

    // Tell the window manager to notify us when the user wants to close the
    // window so we can do it ourselves.
    WM_DELETE_WINDOW = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
    WM_PROTOCOLS     = XInternAtom(mDisplay, "WM_PROTOCOLS", False);
    if (WM_DELETE_WINDOW == None || WM_PROTOCOLS == None)
    {
        destroy();
        return false;
    }

    if (XSetWMProtocols(mDisplay, mWindow, &WM_DELETE_WINDOW, 1) == 0)
    {
        destroy();
        return false;
    }

    // Create an atom to identify our test event
    TEST_EVENT = XInternAtom(mDisplay, "ANGLE_TEST_EVENT", False);
    if (TEST_EVENT == None)
    {
        destroy();
        return false;
    }

    setWindowSizeHints(mDisplay, mWindow, width, height);

    XFlush(mDisplay);

    mX      = 0;
    mY      = 0;
    mWidth  = width;
    mHeight = height;

    return true;
}

void X11Window::disableErrorMessageDialog() {}

void X11Window::destroy()
{
    if (mWindow)
    {
        XDestroyWindow(mDisplay, mWindow);
        XFlush(mDisplay);
        // There appears to be a race condition where XDestroyWindow+XCreateWindow ignores
        // the new size (the same window normally gets reused but this only happens sometimes on
        // some X11 versions). Wait until we get the destroy notification.
        mWindow = 0;  // Set before messageLoop() to avoid a race in processEvent().
        while (!mDestroyed)
        {
            messageLoop();
            angle::Sleep(10);
        }
    }
    if (mDisplay)
    {
        XCloseDisplay(mDisplay);
        mDisplay = nullptr;
    }
    WM_DELETE_WINDOW = None;
    WM_PROTOCOLS     = None;
}

void X11Window::resetNativeWindow() {}

EGLNativeWindowType X11Window::getNativeWindow() const
{
    return mWindow;
}

void *X11Window::getPlatformExtension()
{
    // X11 native window for eglCreateSurfacePlatformEXT is Window*
    return &mWindow;
}

EGLNativeDisplayType X11Window::getNativeDisplay() const
{
    return reinterpret_cast<EGLNativeDisplayType>(mDisplay);
}

void X11Window::messageLoop()
{
    int eventCount = XPending(mDisplay);
    while (eventCount--)
    {
        XEvent event;
        XNextEvent(mDisplay, &event);
        processEvent(event);
    }
}

void X11Window::setMousePosition(int x, int y)
{
    XWarpPointer(mDisplay, None, mWindow, 0, 0, 0, 0, x, y);
}

bool X11Window::setOrientation(int width, int height)
{
    UNIMPLEMENTED();
    return false;
}

bool X11Window::setPosition(int x, int y)
{
    XMoveWindow(mDisplay, mWindow, x, y);
    XFlush(mDisplay);
    return true;
}

bool X11Window::resize(int width, int height)
{
    setWindowSizeHints(mDisplay, mWindow, width, height);
    XResizeWindow(mDisplay, mWindow, width, height);

    XFlush(mDisplay);

    Timer timer;
    timer.start();

    // Wait until the window has actually been resized so that the code calling resize
    // can assume the window has been resized.
    const double kResizeWaitDelay = 0.2;
    while ((mHeight != height || mWidth != width) &&
           timer.getElapsedWallClockTime() < kResizeWaitDelay)
    {
        messageLoop();
        angle::Sleep(10);
    }

    return true;
}

void X11Window::setVisible(bool isVisible)
{
    if (mVisible == isVisible)
    {
        return;
    }

    if (isVisible)
    {
        XMapWindow(mDisplay, mWindow);

        // Wait until we get an event saying this window is mapped so that the
        // code calling setVisible can assume the window is visible.
        // This is important when creating a framebuffer as the framebuffer content
        // is undefined when the window is not visible.
        XEvent placeholderEvent;
        XIfEvent(mDisplay, &placeholderEvent, WaitForMapNotify,
                 reinterpret_cast<XPointer>(mWindow));

        // Block until we get ConfigureNotify to set up fully before returning.
        mConfigured = false;
        while (!mConfigured)
        {
            messageLoop();
            angle::Sleep(10);
        }
    }
    else
    {
        XUnmapWindow(mDisplay, mWindow);
        XFlush(mDisplay);
    }

    mVisible = isVisible;
}

void X11Window::signalTestEvent()
{
    XEvent event;
    event.type                 = ClientMessage;
    event.xclient.message_type = TEST_EVENT;
    // Format needs to be valid or a BadValue is generated
    event.xclient.format = 32;

    // Hijack StructureNotifyMask as we know we will be listening for it.
    XSendEvent(mDisplay, mWindow, False, StructureNotifyMask, &event);

    // For test events, the tests want to check that it really did arrive, and they don't wait
    // long.  XSync here makes sure the event is sent by the time the messageLoop() is called.
    XSync(mDisplay, false);
}

void X11Window::processEvent(const XEvent &xEvent)
{
    // TODO(cwallez) text events
    switch (xEvent.type)
    {
        case ButtonPress:
        {
            Event event;
            angle::MouseButtonType button = angle::MouseButtonType::UNKNOWN;
            int wheelY                    = 0;

            // The mouse wheel updates are sent via button events.
            switch (xEvent.xbutton.button)
            {
                case Button4:
                    wheelY = 1;
                    break;
                case Button5:
                    wheelY = -1;
                    break;
                case 6:
                    break;
                case 7:
                    break;

                case Button1:
                    button = angle::MouseButtonType::LEFT;
                    break;
                case Button2:
                    button = angle::MouseButtonType::MIDDLE;
                    break;
                case Button3:
                    button = angle::MouseButtonType::RIGHT;
                    break;
                case 8:
                    button = angle::MouseButtonType::BUTTON4;
                    break;
                case 9:
                    button = angle::MouseButtonType::BUTTON5;
                    break;

                default:
                    break;
            }

            if (wheelY != 0)
            {
                event.Type             = Event::EVENT_MOUSE_WHEEL_MOVED;
                event.MouseWheel.Delta = wheelY;
                pushEvent(event);
            }

            if (button != angle::MouseButtonType::UNKNOWN)
            {
                event.Type               = Event::EVENT_MOUSE_BUTTON_RELEASED;
                event.MouseButton.Button = button;
                event.MouseButton.X      = xEvent.xbutton.x;
                event.MouseButton.Y      = xEvent.xbutton.y;
                pushEvent(event);
            }
        }
        break;

        case ButtonRelease:
        {
            Event event;
            angle::MouseButtonType button = angle::MouseButtonType::UNKNOWN;

            switch (xEvent.xbutton.button)
            {
                case Button1:
                    button = angle::MouseButtonType::LEFT;
                    break;
                case Button2:
                    button = angle::MouseButtonType::MIDDLE;
                    break;
                case Button3:
                    button = angle::MouseButtonType::RIGHT;
                    break;
                case 8:
                    button = angle::MouseButtonType::BUTTON4;
                    break;
                case 9:
                    button = angle::MouseButtonType::BUTTON5;
                    break;

                default:
                    break;
            }

            if (button != angle::MouseButtonType::UNKNOWN)
            {
                event.Type               = Event::EVENT_MOUSE_BUTTON_RELEASED;
                event.MouseButton.Button = button;
                event.MouseButton.X      = xEvent.xbutton.x;
                event.MouseButton.Y      = xEvent.xbutton.y;
                pushEvent(event);
            }
        }
        break;

        case KeyPress:
        {
            Event event;
            event.Type     = Event::EVENT_KEY_PRESSED;
            event.Key.Code = X11CodeToKey(mDisplay, xEvent.xkey.keycode);
            AddX11KeyStateToEvent(&event, xEvent.xkey.state);
            pushEvent(event);
        }
        break;

        case KeyRelease:
        {
            Event event;
            event.Type     = Event::EVENT_KEY_RELEASED;
            event.Key.Code = X11CodeToKey(mDisplay, xEvent.xkey.keycode);
            AddX11KeyStateToEvent(&event, xEvent.xkey.state);
            pushEvent(event);
        }
        break;

        case EnterNotify:
        {
            Event event;
            event.Type = Event::EVENT_MOUSE_ENTERED;
            pushEvent(event);
        }
        break;

        case LeaveNotify:
        {
            Event event;
            event.Type = Event::EVENT_MOUSE_LEFT;
            pushEvent(event);
        }
        break;

        case MotionNotify:
        {
            Event event;
            event.Type        = Event::EVENT_MOUSE_MOVED;
            event.MouseMove.X = xEvent.xmotion.x;
            event.MouseMove.Y = xEvent.xmotion.y;
            pushEvent(event);
        }
        break;

        case ConfigureNotify:
        {
            mConfigured = true;
            if (mWindow == 0)
            {
                break;
            }
            if (xEvent.xconfigure.width != mWidth || xEvent.xconfigure.height != mHeight)
            {
                Event event;
                event.Type        = Event::EVENT_RESIZED;
                event.Size.Width  = xEvent.xconfigure.width;
                event.Size.Height = xEvent.xconfigure.height;
                pushEvent(event);
            }
            if (xEvent.xconfigure.x != mX || xEvent.xconfigure.y != mY)
            {
                // Sometimes, the window manager reparents our window (for example
                // when resizing) then the X and Y coordinates will be with respect to
                // the new parent and not what the user wants to know. Use
                // XTranslateCoordinates to get the coordinates on the screen.
                int screen  = DefaultScreen(mDisplay);
                Window root = RootWindow(mDisplay, screen);

                int x, y;
                Window child;
                XTranslateCoordinates(mDisplay, mWindow, root, 0, 0, &x, &y, &child);

                if (x != mX || y != mY)
                {
                    Event event;
                    event.Type   = Event::EVENT_MOVED;
                    event.Move.X = x;
                    event.Move.Y = y;
                    pushEvent(event);
                }
            }
        }
        break;

        case FocusIn:
            if (xEvent.xfocus.mode == NotifyNormal || xEvent.xfocus.mode == NotifyWhileGrabbed)
            {
                Event event;
                event.Type = Event::EVENT_GAINED_FOCUS;
                pushEvent(event);
            }
            break;

        case FocusOut:
            if (xEvent.xfocus.mode == NotifyNormal || xEvent.xfocus.mode == NotifyWhileGrabbed)
            {
                Event event;
                event.Type = Event::EVENT_LOST_FOCUS;
                pushEvent(event);
            }
            break;

        case DestroyNotify:
            // Note: we already received WM_DELETE_WINDOW
            mDestroyed = true;
            break;

        case ClientMessage:
            if (xEvent.xclient.message_type == WM_PROTOCOLS &&
                static_cast<Atom>(xEvent.xclient.data.l[0]) == WM_DELETE_WINDOW)
            {
                Event event;
                event.Type = Event::EVENT_CLOSED;
                pushEvent(event);
            }
            else if (xEvent.xclient.message_type == TEST_EVENT)
            {
                Event event;
                event.Type = Event::EVENT_TEST;
                pushEvent(event);
            }
            break;
    }
}

OSWindow *CreateX11Window()
{
    return new X11Window();
}

OSWindow *CreateX11WindowWithVisualId(int visualId)
{
    return new X11Window(visualId);
}

bool IsX11WindowAvailable()
{
    Display *display = XOpenDisplay(nullptr);
    if (!display)
    {
        return false;
    }
    XCloseDisplay(display);
    return true;
}

//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_libc_calls
#endif

#include "OSWindow.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "common/debug.h"
#include "common/system_utils.h"

#if defined(ANGLE_PLATFORM_ANDROID)
#    include "util/android/AndroidWindow.h"
#endif  // defined(ANGLE_PLATFORM_ANDROID)

#ifndef DEBUG_EVENTS
#    define DEBUG_EVENTS 0
#endif

#if DEBUG_EVENTS
static const char *MouseButtonName(angle::MouseButtonType button)
{
    switch (button)
    {
        case angle::MouseButtonType::UNKNOWN:
            return "Unknown";
        case angle::MouseButtonType::LEFT:
            return "Left";
        case angle::MouseButtonType::RIGHT:
            return "Right";
        case angle::MouseButtonType::MIDDLE:
            return "Middle";
        case angle::MouseButtonType::BUTTON4:
            return "Button4";
        case angle::MouseButtonType::BUTTON5:
            return "Button5";
        default:
            UNREACHABLE();
            return nullptr;
    }
}

static const char *KeyName(angle::KeyType key)
{
    switch (key)
    {
        case angle::KeyType::UNKNOWN:
            return "Unknown";
        case angle::KeyType::A:
            return "A";
        case angle::KeyType::B:
            return "B";
        case angle::KeyType::C:
            return "C";
        case angle::KeyType::D:
            return "D";
        case angle::KeyType::E:
            return "E";
        case angle::KeyType::F:
            return "F";
        case angle::KeyType::G:
            return "G";
        case angle::KeyType::H:
            return "H";
        case angle::KeyType::I:
            return "I";
        case angle::KeyType::J:
            return "J";
        case angle::KeyType::K:
            return "K";
        case angle::KeyType::L:
            return "L";
        case angle::KeyType::M:
            return "M";
        case angle::KeyType::N:
            return "N";
        case angle::KeyType::O:
            return "O";
        case angle::KeyType::P:
            return "P";
        case angle::KeyType::Q:
            return "Q";
        case angle::KeyType::R:
            return "R";
        case angle::KeyType::S:
            return "S";
        case angle::KeyType::T:
            return "T";
        case angle::KeyType::U:
            return "U";
        case angle::KeyType::V:
            return "V";
        case angle::KeyType::W:
            return "W";
        case angle::KeyType::X:
            return "X";
        case angle::KeyType::Y:
            return "Y";
        case angle::KeyType::Z:
            return "Z";
        case angle::KeyType::NUM0:
            return "Num0";
        case angle::KeyType::NUM1:
            return "Num1";
        case angle::KeyType::NUM2:
            return "Num2";
        case angle::KeyType::NUM3:
            return "Num3";
        case angle::KeyType::NUM4:
            return "Num4";
        case angle::KeyType::NUM5:
            return "Num5";
        case angle::KeyType::NUM6:
            return "Num6";
        case angle::KeyType::NUM7:
            return "Num7";
        case angle::KeyType::NUM8:
            return "Num8";
        case angle::KeyType::NUM9:
            return "Num9";
        case angle::KeyType::ESCAPE:
            return "Escape";
        case angle::KeyType::LCONTROL:
            return "Left Control";
        case angle::KeyType::LSHIFT:
            return "Left Shift";
        case angle::KeyType::LALT:
            return "Left Alt";
        case angle::KeyType::LSYSTEM:
            return "Left System";
        case angle::KeyType::RCONTROL:
            return "Right Control";
        case angle::KeyType::RSHIFT:
            return "Right Shift";
        case angle::KeyType::RALT:
            return "Right Alt";
        case angle::KeyType::RSYSTEM:
            return "Right System";
        case angle::KeyType::MENU:
            return "Menu";
        case angle::KeyType::LBRACKET:
            return "Left Bracket";
        case angle::KeyType::RBRACKET:
            return "Right Bracket";
        case angle::KeyType::SEMICOLON:
            return "Semicolon";
        case angle::KeyType::COMMA:
            return "Comma";
        case angle::KeyType::PERIOD:
            return "Period";
        case angle::KeyType::QUOTE:
            return "Quote";
        case angle::KeyType::SLASH:
            return "Slash";
        case angle::KeyType::BACKSLASH:
            return "Backslash";
        case angle::KeyType::TILDE:
            return "Tilde";
        case angle::KeyType::EQUAL:
            return "Equal";
        case angle::KeyType::DASH:
            return "Dash";
        case angle::KeyType::SPACE:
            return "Space";
        case angle::KeyType::RETURN:
            return "Return";
        case angle::KeyType::BACK:
            return "Back";
        case angle::KeyType::TAB:
            return "Tab";
        case angle::KeyType::PAGEUP:
            return "Page Up";
        case angle::KeyType::PAGEDOWN:
            return "Page Down";
        case angle::KeyType::END:
            return "End";
        case angle::KeyType::HOME:
            return "Home";
        case angle::KeyType::INSERT:
            return "Insert";
        case angle::KeyType::DEL:
            return "Delete";
        case angle::KeyType::ADD:
            return "Add";
        case angle::KeyType::SUBTRACT:
            return "Substract";
        case angle::KeyType::MULTIPLY:
            return "Multiply";
        case angle::KeyType::DIVIDE:
            return "Divide";
        case angle::KeyType::LEFT:
            return "Left";
        case angle::KeyType::RIGHT:
            return "Right";
        case angle::KeyType::UP:
            return "Up";
        case angle::KeyType::DOWN:
            return "Down";
        case angle::KeyType::NUMPAD0:
            return "Numpad 0";
        case angle::KeyType::NUMPAD1:
            return "Numpad 1";
        case angle::KeyType::NUMPAD2:
            return "Numpad 2";
        case angle::KeyType::NUMPAD3:
            return "Numpad 3";
        case angle::KeyType::NUMPAD4:
            return "Numpad 4";
        case angle::KeyType::NUMPAD5:
            return "Numpad 5";
        case angle::KeyType::NUMPAD6:
            return "Numpad 6";
        case angle::KeyType::NUMPAD7:
            return "Numpad 7";
        case angle::KeyType::NUMPAD8:
            return "Numpad 8";
        case angle::KeyType::NUMPAD9:
            return "Numpad 9";
        case angle::KeyType::F1:
            return "F1";
        case angle::KeyType::F2:
            return "F2";
        case angle::KeyType::F3:
            return "F3";
        case angle::KeyType::F4:
            return "F4";
        case angle::KeyType::F5:
            return "F5";
        case angle::KeyType::F6:
            return "F6";
        case angle::KeyType::F7:
            return "F7";
        case angle::KeyType::F8:
            return "F8";
        case angle::KeyType::F9:
            return "F9";
        case angle::KeyType::F10:
            return "F10";
        case angle::KeyType::F11:
            return "F11";
        case angle::KeyType::F12:
            return "F12";
        case angle::KeyType::F13:
            return "F13";
        case angle::KeyType::F14:
            return "F14";
        case angle::KeyType::F15:
            return "F15";
        case angle::KeyType::PAUSE:
            return "Pause";
        default:
            return "Unknown Key";
    }
}

static std::string KeyState(const Event::KeyEvent &event)
{
    if (event.Shift || event.Control || event.Alt || event.System)
    {
        std::ostringstream buffer;
        buffer << " [";

        if (event.Shift)
        {
            buffer << "Shift";
        }
        if (event.Control)
        {
            buffer << "Control";
        }
        if (event.Alt)
        {
            buffer << "Alt";
        }
        if (event.System)
        {
            buffer << "System";
        }

        buffer << "]";
        return buffer.str();
    }
    return "";
}

static void PrintEvent(const Event &event)
{
    switch (event.Type)
    {
        case Event::EVENT_CLOSED:
            std::cout << "Event: Window Closed" << std::endl;
            break;
        case Event::EVENT_MOVED:
            std::cout << "Event: Window Moved (" << event.Move.X << ", " << event.Move.Y << ")"
                      << std::endl;
            break;
        case Event::EVENT_RESIZED:
            std::cout << "Event: Window Resized (" << event.Size.Width << ", " << event.Size.Height
                      << ")" << std::endl;
            break;
        case Event::EVENT_LOST_FOCUS:
            std::cout << "Event: Window Lost Focus" << std::endl;
            break;
        case Event::EVENT_GAINED_FOCUS:
            std::cout << "Event: Window Gained Focus" << std::endl;
            break;
        case Event::EVENT_TEXT_ENTERED:
            // TODO(cwallez) show the character
            std::cout << "Event: Text Entered" << std::endl;
            break;
        case Event::EVENT_KEY_PRESSED:
            std::cout << "Event: Key Pressed (" << KeyName(event.Key.Code) << KeyState(event.Key)
                      << ")" << std::endl;
            break;
        case Event::EVENT_KEY_RELEASED:
            std::cout << "Event: Key Released (" << KeyName(event.Key.Code) << KeyState(event.Key)
                      << ")" << std::endl;
            break;
        case Event::EVENT_MOUSE_WHEEL_MOVED:
            std::cout << "Event: Mouse Wheel (" << event.MouseWheel.Delta << ")" << std::endl;
            break;
        case Event::EVENT_MOUSE_BUTTON_PRESSED:
            std::cout << "Event: Mouse Button Pressed " << MouseButtonName(event.MouseButton.Button)
                      << " at (" << event.MouseButton.X << ", " << event.MouseButton.Y << ")"
                      << std::endl;
            break;
        case Event::EVENT_MOUSE_BUTTON_RELEASED:
            std::cout << "Event: Mouse Button Released "
                      << MouseButtonName(event.MouseButton.Button) << " at (" << event.MouseButton.X
                      << ", " << event.MouseButton.Y << ")" << std::endl;
            break;
        case Event::EVENT_MOUSE_MOVED:
            std::cout << "Event: Mouse Moved (" << event.MouseMove.X << ", " << event.MouseMove.Y
                      << ")" << std::endl;
            break;
        case Event::EVENT_MOUSE_ENTERED:
            std::cout << "Event: Mouse Entered Window" << std::endl;
            break;
        case Event::EVENT_MOUSE_LEFT:
            std::cout << "Event: Mouse Left Window" << std::endl;
            break;
        case Event::EVENT_TEST:
            std::cout << "Event: Test" << std::endl;
            break;
        default:
            UNREACHABLE();
            break;
    }
}
#endif

OSWindow::OSWindow() : mX(0), mY(0), mWidth(0), mHeight(0), mValid(false), mIgnoreSizeEvents(false)
{}

OSWindow::~OSWindow() {}

bool OSWindow::initialize(const std::string &name, int width, int height)
{
    mValid = initializeImpl(name, width, height);
    return mValid;
}

int OSWindow::getX() const
{
    return mX;
}

int OSWindow::getY() const
{
    return mY;
}

int OSWindow::getWidth() const
{
    return mWidth;
}

int OSWindow::getHeight() const
{
    return mHeight;
}

bool OSWindow::takeScreenshot(uint8_t *pixelData)
{
    return false;
}

void *OSWindow::getPlatformExtension()
{
    return reinterpret_cast<void *>(getNativeWindow());
}

bool OSWindow::popEvent(Event *event)
{
    if (mEvents.size() > 0 && event)
    {
        *event = mEvents.front();
        mEvents.pop_front();
        return true;
    }
    else
    {
        return false;
    }
}

void OSWindow::pushEvent(Event event)
{
    switch (event.Type)
    {
        case Event::EVENT_MOVED:
            mX = event.Move.X;
            mY = event.Move.Y;
            break;
        case Event::EVENT_RESIZED:
            mWidth  = event.Size.Width;
            mHeight = event.Size.Height;
            break;
        default:
            break;
    }

    mEvents.push_back(event);

#if DEBUG_EVENTS
    PrintEvent(event);
#endif
}

bool OSWindow::didTestEventFire()
{
    Event topEvent;
    while (popEvent(&topEvent))
    {
        if (topEvent.Type == Event::EVENT_TEST)
        {
            return true;
        }
    }

    return false;
}

// static
void OSWindow::Delete(OSWindow **window)
{
    delete *window;
    *window = nullptr;
}

namespace angle
{
bool FindTestDataPath(const char *searchPath, char *dataPathOut, size_t maxDataPathOutLen)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    const std::string searchPaths[] = {
        AndroidWindow::GetExternalStorageDirectory(),
        AndroidWindow::GetExternalStorageDirectory() + "/third_party/angle",
        AndroidWindow::GetApplicationDirectory() + "/chromium_tests_root"};
#elif ANGLE_PLATFORM_IOS_FAMILY
    const std::string searchPaths[] = {GetExecutableDirectory(),
                                       GetExecutableDirectory() + "/third_party/angle"};
#else
    const std::string searchPaths[] = {
        GetExecutableDirectory(), GetExecutableDirectory() + "/../..", ".",
        GetExecutableDirectory() + "/../../third_party/angle", "third_party/angle"};
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    for (const std::string &path : searchPaths)
    {
        std::stringstream pathStream;
        pathStream << path << "/" << searchPath;
        std::string candidatePath = pathStream.str();

        if (candidatePath.size() + 1 >= maxDataPathOutLen)
        {
            ERR() << "FindTestDataPath: Path too long.";
            return false;
        }

        if (angle::IsDirectory(candidatePath.c_str()))
        {
            memcpy(dataPathOut, candidatePath.c_str(), candidatePath.size() + 1);
            return true;
        }

        std::ifstream inFile(candidatePath.c_str());
        if (!inFile.fail())
        {
            memcpy(dataPathOut, candidatePath.c_str(), candidatePath.size() + 1);
            return true;
        }
    }

    return false;
}
}  // namespace angle

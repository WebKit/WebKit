//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// window_system_unittest.cpp: Unit tests for the X11-vs-Wayland selection policy.

#include "common/linux/window_system.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "common/system_utils.h"

namespace angle
{
namespace
{
// Clears the session variables and restores them on destruction, so tests do
// not depend on the session the test runner itself is in.
class ScopedSessionEnvironment
{
  public:
    ScopedSessionEnvironment()
    {
        for (const char *name : kSessionVars)
        {
            mSaved.emplace_back(name, GetEnvironmentVar(name));
            UnsetEnvironmentVar(name);
        }
    }

    ~ScopedSessionEnvironment()
    {
        for (const auto &[name, value] : mSaved)
        {
            if (value.empty())
            {
                UnsetEnvironmentVar(name);
            }
            else
            {
                SetEnvironmentVar(name, value.c_str());
            }
        }
    }

  private:
    static constexpr const char *kSessionVars[] = {"WAYLAND_DISPLAY", "XDG_SESSION_TYPE",
                                                   "DESKTOP_SESSION"};
    std::vector<std::pair<const char *, std::string>> mSaved;
};

// Both backends compiled: the preference wins.
TEST(WindowSystemTest, BothCompiled)
{
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/true,
                                                             /*preferred=*/WindowSystem::X11));
    EXPECT_EQ(WindowSystem::Wayland,
              ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/true,
                                          /*preferred=*/WindowSystem::Wayland));
}

// Only one backend compiled: it is always chosen regardless of the environment.
TEST(WindowSystemTest, SingleBackend)
{
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/false,
                                                             /*preferred=*/WindowSystem::X11));
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/false,
                                                             /*preferred=*/WindowSystem::Wayland));
    EXPECT_EQ(WindowSystem::Wayland, ChoosePreferredWindowSystem(/*x11=*/false, /*wayland=*/true,
                                                                 /*preferred=*/WindowSystem::X11));
    EXPECT_EQ(WindowSystem::Wayland,
              ChoosePreferredWindowSystem(/*x11=*/false, /*wayland=*/true,
                                          /*preferred=*/WindowSystem::Wayland));
}

// Neither backend compiled: nothing can be selected.
TEST(WindowSystemTest, NeitherCompiled)
{
    EXPECT_EQ(WindowSystem::Unspecified,
              ChoosePreferredWindowSystem(/*x11=*/false, /*wayland=*/false,
                                          /*preferred=*/WindowSystem::X11));
    EXPECT_EQ(WindowSystem::Unspecified,
              ChoosePreferredWindowSystem(/*x11=*/false, /*wayland=*/false,
                                          /*preferred=*/WindowSystem::Wayland));
}

// An X11 session, or no session information at all, is not a Wayland session.
TEST(WindowSystemTest, SessionNotWayland)
{
    ScopedSessionEnvironment scopedEnvironment;

    EXPECT_FALSE(IsWaylandSession());

    SetEnvironmentVar("XDG_SESSION_TYPE", "x11");
    SetEnvironmentVar("DESKTOP_SESSION", "plasmax11");
    EXPECT_FALSE(IsWaylandSession());
}

// Any one of the session variables on its own marks a Wayland session, which is
// what keeps this in agreement with rx::IsXWayland().
TEST(WindowSystemTest, SessionWayland)
{
    ScopedSessionEnvironment scopedEnvironment;

    SetEnvironmentVar("WAYLAND_DISPLAY", "wayland-1");
    EXPECT_TRUE(IsWaylandSession());
    UnsetEnvironmentVar("WAYLAND_DISPLAY");

    SetEnvironmentVar("XDG_SESSION_TYPE", "wayland");
    EXPECT_TRUE(IsWaylandSession());
    UnsetEnvironmentVar("XDG_SESSION_TYPE");

    SetEnvironmentVar("DESKTOP_SESSION", "plasmawayland");
    EXPECT_TRUE(IsWaylandSession());
}

// The session hints must not pick a backend. A Wayland session reached through
// XWayland exports no WAYLAND_DISPLAY, so it is a Wayland session but resolves
// to X11 when both backends are compiled.
TEST(WindowSystemTest, SessionHintsDoNotSelectWayland)
{
    ScopedSessionEnvironment scopedEnvironment;

    SetEnvironmentVar("XDG_SESSION_TYPE", "wayland");
    SetEnvironmentVar("DESKTOP_SESSION", "plasmawayland");
    EXPECT_TRUE(IsWaylandSession());
    EXPECT_FALSE(IsWaylandDisplay());
    EXPECT_EQ(WindowSystem::X11, GetWindowSystemFromEnvironment());
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/true,
                                                             GetWindowSystemFromEnvironment()));

    // WAYLAND_DISPLAY is what moves selection to Wayland.
    SetEnvironmentVar("WAYLAND_DISPLAY", "wayland-1");
    EXPECT_TRUE(IsWaylandDisplay());
    EXPECT_EQ(WindowSystem::Wayland, GetWindowSystemFromEnvironment());
    EXPECT_EQ(WindowSystem::Wayland, ChoosePreferredWindowSystem(/*x11=*/true, /*wayland=*/true,
                                                                 GetWindowSystemFromEnvironment()));
}

// A preference for a backend that is not compiled in falls back to one that is.
TEST(WindowSystemTest, PreferenceNotCompiled)
{
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(
                                     /*x11=*/true, /*wayland=*/false,
                                     /*preferred=*/WindowSystem::Wayland));
    EXPECT_EQ(WindowSystem::Wayland, ChoosePreferredWindowSystem(
                                         /*x11=*/false, /*wayland=*/true,
                                         /*preferred=*/WindowSystem::X11));
    EXPECT_EQ(WindowSystem::X11, ChoosePreferredWindowSystem(
                                     /*x11=*/true, /*wayland=*/true,
                                     /*preferred=*/WindowSystem::Unspecified));
}
}  // namespace
}  // namespace angle

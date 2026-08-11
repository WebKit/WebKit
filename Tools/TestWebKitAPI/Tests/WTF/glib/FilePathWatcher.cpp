/*
 * Copyright (C) 2026 Igalia S.L.
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
#include <wtf/glib/FilePathWatcher.h>

#include "GLibMainLoopUtilities.h"
#include "Helpers/Test.h"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

namespace {

class TempDir {
public:
    TempDir()
    {
        GUniqueOutPtr<GError> error;
        m_path.reset(g_dir_make_tmp("wtf-fpw-XXXXXX", &error.outPtr()));
    }

    // Remove any leftover entries before rmdir so the directory is cleaned up
    // even when a test fails partway through (the tests create no subdirs).
    ~TempDir()
    {
        if (!m_path)
            return;
        if (GDir* dir = g_dir_open(m_path.get(), 0, nullptr)) {
            while (const char* name = g_dir_read_name(dir)) {
                GUniquePtr<char> child(g_build_filename(m_path.get(), name, nullptr));
                g_unlink(child.get());
            }
            g_dir_close(dir);
        }
        g_rmdir(m_path.get());
    }

    const char* path() const { return m_path.get(); }
    bool isValid() const { return !!m_path; }

private:
    GUniquePtr<char> m_path;
};

String writeAndReturnPath(const char* dir, const char* name, std::span<const uint8_t> contents)
{
    GUniquePtr<char> joined(g_build_filename(dir, name, nullptr));
    String path = String::fromUTF8(joined.get());
    FileSystem::overwriteEntireFile(path, contents);
    return path;
}

} // namespace

TEST(WTF_FilePathWatcher, FiresOnWrite)
{
    TempDir dir;
    ASSERT_TRUE(dir.isValid());

    // Seed the file before watching: the watcher suppresses bare DELETED
    // events that fire mid-rewrite (see FilePathWatcher.cpp), so we want a
    // CHANGES_DONE_HINT terminator on the next write.
    String watched = writeAndReturnPath(dir.path(), "watched", std::span<const uint8_t> { });

    bool fired = false;
    FilePathWatcher watcher(watched, [&] {
        fired = true;
    });
    EXPECT_TRUE(watcher.isActive());

    auto contents = String::fromLatin1("changed").utf8();
    FileSystem::overwriteEntireFile(watched, byteCast<uint8_t>(contents.span()));

    EXPECT_TRUE(runMainLoopUntil([&] {
        return fired;
    }));
}

// Simulates the /etc/localtime symlink-swap that timedated performs: first
// remove the existing symlink, then re-create it pointing at a new target.
// GIO terminates this with CREATED, which the watcher honors.
TEST(WTF_FilePathWatcher, FiresOnSymlinkSwap)
{
    TempDir dir;
    ASSERT_TRUE(dir.isValid());

    String targetA = writeAndReturnPath(dir.path(), "tz-a", std::span<const uint8_t> { });
    String targetB = writeAndReturnPath(dir.path(), "tz-b", std::span<const uint8_t> { });
    GUniquePtr<char> linkPath(g_build_filename(dir.path(), "localtime", nullptr));

    if (symlink(targetA.utf8().data(), linkPath.get()) < 0)
        GTEST_SKIP() << "symlink() unavailable on this filesystem";

    bool fired = false;
    FilePathWatcher watcher(String::fromUTF8(linkPath.get()), [&] {
        fired = true;
    });
    EXPECT_TRUE(watcher.isActive());

    g_unlink(linkPath.get());
    ASSERT_EQ(symlink(targetB.utf8().data(), linkPath.get()), 0);

    EXPECT_TRUE(runMainLoopUntil([&] {
        return fired;
    }));
}

TEST(WTF_FilePathWatcher, IgnoresUnrelatedSiblings)
{
    TempDir dir;
    ASSERT_TRUE(dir.isValid());

    String watched = writeAndReturnPath(dir.path(), "watched", std::span<const uint8_t> { });
    String sibling = writeAndReturnPath(dir.path(), "sibling", std::span<const uint8_t> { });

    bool fired = false;
    FilePathWatcher watcher(watched, [&] {
        fired = true;
    });
    EXPECT_TRUE(watcher.isActive());

    auto contents = String::fromLatin1("noise").utf8();
    FileSystem::overwriteEntireFile(sibling, byteCast<uint8_t>(contents.span()));

    // 1s of pumping is more than enough for any real notification to land;
    // we expect timeout (predicate stays false) which is what we want.
    EXPECT_FALSE(runMainLoopUntil([&] {
        return fired;
    }, /* timeoutSeconds */ 1));
}

} // namespace TestWebKitAPI

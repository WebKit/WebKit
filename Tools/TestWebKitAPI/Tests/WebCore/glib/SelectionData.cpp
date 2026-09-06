/*
 * Copyright (C) 2026 Hayden Barnes. All rights reserved.
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

#if PLATFORM(GTK) || PLATFORM(WPE)

#include "Helpers/Test.h"

#include <WebCore/DragData.h>
#include <WebCore/SelectionData.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {
using namespace WebCore;

// A web-authored uri-list must not become filenames.

TEST(SelectionData, SetURIListDoesNotPromoteFilenames)
{
    SelectionData data;
    data.setURIList("file:///etc/passwd\r\nhttps://example.com/\r\n"_s);

    EXPECT_TRUE(data.hasURIList());
    EXPECT_TRUE(data.hasURL());
    EXPECT_EQ(data.url().string(), "file:///etc/passwd"_s);
    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(data.filenames().isEmpty());
}

TEST(SelectionData, SetURIListKeepsHttpURLWithoutFilenames)
{
    SelectionData data;
    data.setURIList("https://example.com/path\n"_s);

    EXPECT_TRUE(data.hasURL());
    EXPECT_EQ(data.url().string(), "https://example.com/path"_s);
    EXPECT_FALSE(data.hasFilenames());
}

TEST(SelectionData, TrustedSetFilenamesFromURIList)
{
    SelectionData data;
    data.setURIList("file:///tmp/trusted.txt\r\nhttps://example.com/\r\n"_s);
    EXPECT_FALSE(data.hasFilenames());

    data.setFilenamesFromURIList(data.uriList());
    EXPECT_TRUE(data.hasFilenames());
    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/tmp/trusted.txt"_s);
}

TEST(SelectionData, ExplicitSetFilenames)
{
    SelectionData data;
    data.setURIList("file:///tmp/ignored-for-files.txt"_s);
    data.setFilenames(Vector<String> { "/home/user/document.pdf"_s, "/home/user/photo.png"_s });

    ASSERT_EQ(data.filenames().size(), 2u);
    EXPECT_EQ(data.filenames()[0], "/home/user/document.pdf"_s);
    EXPECT_EQ(data.filenames()[1], "/home/user/photo.png"_s);
}

TEST(SelectionData, FilenamesFromURIListSkipsCommentsAndNonFiles)
{
    auto filenames = SelectionData::filenamesFromURIList(
        "# comment\n"
        "https://example.com/a\n"
        "file:///tmp/a.txt\n"
        "\n"
        "file:///tmp/b.txt\r\n"_s);

    ASSERT_EQ(filenames.size(), 2u);
    EXPECT_EQ(filenames[0], "/tmp/a.txt"_s);
    EXPECT_EQ(filenames[1], "/tmp/b.txt"_s);
}

TEST(SelectionData, ClearFilenames)
{
    SelectionData data;
    data.setFilenames(Vector<String> { "/tmp/x"_s });
    EXPECT_TRUE(data.hasFilenames());
    data.clearFilenames();
    EXPECT_FALSE(data.hasFilenames());
}

TEST(SelectionData, URIListWithoutFilenamesStripsFileURLs)
{
    auto sanitized = SelectionData::uriListWithoutFilenames(
        "file:///etc/passwd\r\nhttps://example.com/a\r\nfile:///tmp/x\r\n"_s);
    EXPECT_EQ(sanitized, "https://example.com/a"_s);
}

TEST(SelectionData, URIListWithoutFilenamesEmptyWhenOnlyFiles)
{
    auto sanitized = SelectionData::uriListWithoutFilenames("file:///tmp/only\n"_s);
    EXPECT_TRUE(sanitized.isEmpty());
}

// Mirrors SelectionData.serialization.in decode: filenames survive without setURIList promotion.
TEST(SelectionData, IpcConstructorPreservesFilenamesWithoutURIListPromotion)
{
    SelectionData decoded(
        String(),
        String(),
        URL(),
        "file:///etc/passwd\r\nhttps://example.com/\r\n"_s,
        Vector<String> { "/tmp/trusted-drop.txt"_s },
        nullptr,
        nullptr,
        false);

    EXPECT_TRUE(decoded.hasURIList());
    EXPECT_TRUE(decoded.hasFilenames());
    ASSERT_EQ(decoded.filenames().size(), 1u);
    EXPECT_EQ(decoded.filenames()[0], "/tmp/trusted-drop.txt"_s);

    SelectionData uriOnly(
        String(),
        String(),
        URL(),
        "file:///etc/passwd\r\n"_s,
        Vector<String> { },
        nullptr,
        nullptr,
        false);
    EXPECT_TRUE(uriOnly.hasURIList());
    EXPECT_FALSE(uriOnly.hasFilenames());
}

TEST(SelectionData, DragDataIsSourceDeniesFilenameAccess)
{
    SelectionData selection;
    selection.setFilenames(Vector<String> { "/tmp/trusted-looking.txt"_s });

    DragData external(&selection, { }, { }, { });
    EXPECT_TRUE(external.containsFiles());
    EXPECT_EQ(external.numberOfFiles(), 1u);

    DragData local(&selection, { }, { }, { }, DragApplicationFlags::IsSource);
    EXPECT_FALSE(local.containsFiles());
    EXPECT_EQ(local.numberOfFiles(), 0u);
    EXPECT_TRUE(local.asFilenames().isEmpty());
}

// The portal list wins over a parallel hostile uri-list.
TEST(SelectionData, PortalFilenamesNotWidenedByHostileURIList)
{
    SelectionData data;
    data.setURIList("file:///etc/passwd\r\nhttps://example.com/\r\nfile:///tmp/extra-from-uri-list.txt\r\n"_s);
    data.setFilenames(Vector<String> { "/run/user/1000/doc/portal-only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/portal-only.txt"_s);
    EXPECT_FALSE(data.filenames().contains("/etc/passwd"_s));
    EXPECT_FALSE(data.filenames().contains("/tmp/extra-from-uri-list.txt"_s));
    EXPECT_TRUE(data.hasURIList());
}

// Export sanitize contract used by DragSource and clipboard write.
TEST(SelectionData, UriListWithoutFilenamesKeepsHttpOnly)
{
    auto out = SelectionData::uriListWithoutFilenames(
        "file:///etc/passwd\r\nhttps://example.com/a\r\nhttp://example.org/b\r\nfile:///tmp/x\r\n"_s);
    EXPECT_TRUE(out.contains("https://example.com/a"_s));
    EXPECT_TRUE(out.contains("http://example.org/b"_s));
    EXPECT_FALSE(out.contains("file://"_s));
    EXPECT_FALSE(out.contains("passwd"_s));
}

// An external drop after IPC: containsFiles only when not IsSource.
TEST(SelectionData, TrustedDropShapeAfterIpcRoundTrip)
{
    SelectionData decoded(
        String(),
        String(),
        URL(),
        "file:///home/user/doc.txt\r\n"_s,
        Vector<String> { "/home/user/doc.txt"_s },
        nullptr,
        nullptr,
        false);

    EXPECT_TRUE(decoded.hasFilenames());
    DragData external(&decoded, { }, { }, { });
    EXPECT_TRUE(external.containsFiles());
    EXPECT_EQ(external.numberOfFiles(), 1u);

    DragData asSource(&decoded, { }, { }, { }, DragApplicationFlags::IsSource);
    EXPECT_FALSE(asSource.containsFiles());
}

// setTrustedDrop is the one entry point, so GTK3 and GTK4 cannot drift.

TEST(SelectionData, TrustedDropPrefersPortalFilenames)
{
    SelectionData data;
    data.setTrustedDrop("file:///etc/passwd\r\nfile:///tmp/extra.txt\r\n"_s,
        Vector<String> { "/run/user/1000/doc/portal-only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/portal-only.txt"_s);
    EXPECT_FALSE(data.filenames().contains("/etc/passwd"_s));
    EXPECT_FALSE(data.filenames().contains("/tmp/extra.txt"_s));
}

TEST(SelectionData, TrustedDropFallsBackToURIListWhenNoPortalList)
{
    SelectionData data;
    data.setTrustedDrop("file:///home/user/a.txt\r\nfile:///home/user/b.txt\r\nhttps://example.com/\r\n"_s, { });

    ASSERT_EQ(data.filenames().size(), 2u);
    EXPECT_EQ(data.filenames()[0], "/home/user/a.txt"_s);
    EXPECT_EQ(data.filenames()[1], "/home/user/b.txt"_s);
    EXPECT_TRUE(data.hasURIList());
}

TEST(SelectionData, TrustedDropWithoutFilesGrantsNothing)
{
    SelectionData data;
    data.setTrustedDrop("https://example.com/\r\n"_s, { });

    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(data.hasURL());
}

TEST(SelectionData, TrustedDropWithOnlyPortalListHasNoURIList)
{
    SelectionData data;
    data.setTrustedDrop(emptyString(), Vector<String> { "/run/user/1000/doc/only.txt"_s });

    ASSERT_EQ(data.filenames().size(), 1u);
    EXPECT_EQ(data.filenames()[0], "/run/user/1000/doc/only.txt"_s);
    EXPECT_FALSE(data.hasURIList());
}

// A second drop must not inherit the previous grant.
TEST(SelectionData, TrustedDropReplacesPreviousFilenames)
{
    SelectionData data;
    data.setTrustedDrop("file:///home/user/first.txt\r\n"_s, { });
    ASSERT_EQ(data.filenames().size(), 1u);

    data.setTrustedDrop("https://example.com/\r\n"_s, { });
    EXPECT_FALSE(data.hasFilenames());
}

// WebPageProxy::startDrag() clears filenames on the way up. The rest of the drag survives.
TEST(SelectionData, ClearFilenamesKeepsTheRestOfTheDrag)
{
    SelectionData data;
    data.setText("dragged text"_s);
    data.setURIList("file:///home/user/secret.txt\r\nhttps://example.com/\r\n"_s);
    data.setFilenames(Vector<String> { "/home/user/secret.txt"_s });
    ASSERT_TRUE(data.hasFilenames());

    data.clearFilenames();

    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(data.hasText());
    EXPECT_TRUE(data.hasURIList());
}

// file://attacker.example/etc/passwd must not resolve to /etc/passwd.
TEST(SelectionData, RemoteHostFileURIIsNotAGrant)
{
    SelectionData data;
    data.setTrustedDrop("file://attacker.example/etc/passwd\r\n"_s, { });
    EXPECT_FALSE(data.hasFilenames());
}

// An empty authority is the ordinary form and must keep working.
TEST(SelectionData, EmptyAuthorityFileURIIsAGrant)
{
    SelectionData data;
    data.setTrustedDrop("file:///etc/hostname\r\n"_s, { });
    ASSERT_TRUE(data.hasFilenames());
    EXPECT_EQ(1u, data.filenames().size());
    EXPECT_STREQ("/etc/hostname", data.filenames()[0].utf8().data());
}

// RFC 8089: localhost means this machine, case insensitive.
TEST(SelectionData, LocalhostAuthorityFileURIIsAGrant)
{
    SelectionData data;
    data.setTrustedDrop("file://localhost/etc/hostname\r\n"_s, { });
    ASSERT_TRUE(data.hasFilenames());
    EXPECT_EQ(1u, data.filenames().size());
    EXPECT_STREQ("/etc/hostname", data.filenames()[0].utf8().data());

    SelectionData upper;
    upper.setTrustedDrop("file://LOCALHOST/etc/hostname\r\n"_s, { });
    EXPECT_TRUE(upper.hasFilenames());
}

// Not a grant, but still not something to hand another application. Export is
// broader than import.
TEST(SelectionData, RemoteHostFileURIIsStrippedOnExport)
{
    auto stripped = SelectionData::uriListWithoutFilenames("file://attacker.example/etc/passwd\r\nhttps://webkit.org/\r\n"_s);
    EXPECT_STREQ("https://webkit.org/", stripped.utf8().data());
}

// Whatever import grants, export must strip.
TEST(SelectionData, EverythingGrantedOnImportIsStrippedOnExport)
{
    const char* lines[] = {
        "file:///etc/hostname",
        "file://localhost/etc/hostname",
        "file://LOCALHOST/etc/hostname",
        "file://attacker.example/etc/passwd",
        "file:///home/user/a b c.txt",
        "file:///home/user/%C3%A9.txt",
        "https://webkit.org/",
        "mailto:nobody@webkit.org",
        "not a uri at all",
    };

    for (auto* line : lines) {
        auto uriList = makeString(String::fromUTF8(line), "\r\n"_s);

        SelectionData data;
        data.setTrustedDrop(uriList, { });

        auto stripped = SelectionData::uriListWithoutFilenames(uriList);
        bool survivedExport = stripped.contains(String::fromUTF8(line));

        if (data.hasFilenames())
            EXPECT_FALSE(survivedExport) << "granted but not stripped: " << line;
    }
}

// Pasteboard::fileContentState() uses this to narrow the type list, never to widen it.
TEST(SelectionData, URIListContainsFileURI)
{
    EXPECT_TRUE(SelectionData::uriListContainsFileURI("file:///home/user/a.txt\r\n"_s));
    EXPECT_TRUE(SelectionData::uriListContainsFileURI("https://webkit.org/\r\nfile:///home/user/a.txt\r\n"_s));

    EXPECT_FALSE(SelectionData::uriListContainsFileURI("https://webkit.org/\r\n"_s));
    EXPECT_FALSE(SelectionData::uriListContainsFileURI("# file:///home/user/a.txt\r\n"_s));
    EXPECT_FALSE(SelectionData::uriListContainsFileURI(emptyString()));
}

// Host authority does not matter for narrowing.
TEST(SelectionData, URIListContainsFileURICountsRemoteHosts)
{
    EXPECT_TRUE(SelectionData::uriListContainsFileURI("file://attacker.example/etc/passwd\r\n"_s));

    SelectionData data;
    data.setTrustedDrop("file://attacker.example/etc/passwd\r\n"_s, { });
    EXPECT_FALSE(data.hasFilenames());
}

// A dragged local image has no filenames, but setURL() puts its src in the uri-list
// and the text.
TEST(SelectionData, LocalImageDragHasNoFilenamesButAFileURIList)
{
    SelectionData data;
    data.setURL(URL { "file:///home/user/greenbox.png"_s }, emptyString());

    EXPECT_FALSE(data.hasFilenames());
    EXPECT_TRUE(SelectionData::uriListContainsFileURI(data.uriList()));
    EXPECT_EQ(data.text(), "file:///home/user/greenbox.png"_s);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(GTK) || PLATFORM(WPE)

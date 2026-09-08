/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebViewTest.h"
#include <glib/gstdio.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>

// Each test isolates one defence. Revert that defence and the test must fail.

class DragAndDropTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(DragAndDropTest);

    DragAndDropTest()
    {
        m_tempDirectory.reset(g_dir_make_tmp("WebKitDragAndDropTest-XXXXXX", nullptr));
        g_assert_nonnull(m_tempDirectory.get());
    }

    ~DragAndDropTest()
    {
        for (const auto& path : m_files)
            g_unlink(path.utf8().data());
        if (m_tempDirectory)
            g_rmdir(m_tempDirectory.get());
    }

    // A real file, so a denied drop cannot be mistaken for a missing one.
    String createFile(const char* name, const char* contents)
    {
        GUniquePtr<char> path(g_build_filename(m_tempDirectory.get(), name, nullptr));
        g_assert_true(g_file_set_contents(path.get(), contents, -1, nullptr));
        String result = String::fromUTF8(path.get());
        m_files.append(result);
        return result;
    }

    static String uriForPath(const String& path)
    {
        GUniquePtr<char> uri(g_filename_to_uri(path.utf8().data(), nullptr, nullptr));
        g_assert_nonnull(uri.get());
        return String::fromUTF8(uri.get());
    }

    // Installs a drop handler that records what dataTransfer exposed to the page.
    void loadDropTarget()
    {
        static const char* html =
            "<html><body style='margin:0'>"
            "<div id='target' style='width:200px;height:200px'></div>"
            "<script>"
            "window.dropFileCount = -1;"
            "window.dropFileNames = '';"
            "window.dropFileContents = '';"
            "document.addEventListener('dragover', e => e.preventDefault());"
            "document.addEventListener('drop', e => {"
            "  e.preventDefault();"
            "  const files = e.dataTransfer.files;"
            "  window.dropFileCount = files.length;"
            "  window.dropFileNames = Array.from(files).map(f => f.name).join(',');"
            "  let pending = files.length;"
            "  if (!pending) { window.dropDone = true; return; }"
            "  const parts = new Array(pending);"
            "  Array.from(files).forEach((f, i) => {"
            "    const reader = new FileReader();"
            "    reader.onloadend = () => {"
            "      parts[i] = reader.result || '';"
            "      if (!--pending) {"
            "        window.dropFileContents = parts.join('|');"
            "        window.dropDone = true;"
            "      }"
            "    };"
            "    reader.readAsText(f);"
            "  });"
            "});"
            "</script></body></html>";

        loadHtml(html, nullptr);
        waitUntilLoadFinished();
        showInWindow(200, 200);
    }

    double numberByEvaluatingJavaScript(const char* script)
    {
        GUniqueOutPtr<GError> error;
        auto* value = runJavaScriptAndWaitUntilFinished(script, &error.outPtr());
        g_assert_no_error(error.get());
        g_assert_nonnull(value);
        g_assert_true(jsc_value_is_number(value));
        return jsc_value_to_double(value);
    }

    String stringByEvaluatingJavaScript(const char* script)
    {
        GUniqueOutPtr<GError> error;
        auto* value = runJavaScriptAndWaitUntilFinished(script, &error.outPtr());
        g_assert_no_error(error.get());
        g_assert_nonnull(value);
        GUniquePtr<char> string(jsc_value_to_string(value));
        return String::fromUTF8(string.get());
    }

    // The drop and FileReader are both asynchronous, so poll.
    void waitUntilDropHandled()
    {
        for (unsigned i = 0; i < 200; ++i) {
            if (numberByEvaluatingJavaScript("window.dropDone ? 1 : 0") > 0)
                return;
            g_usleep(10000);
        }
        g_assert_not_reached();
    }

    int droppedFileCount() { return static_cast<int>(numberByEvaluatingJavaScript("window.dropFileCount")); }

    GUniquePtr<char> m_tempDirectory;
    Vector<String> m_files;
};

// The CVE-2025-13947 mechanism. No IsSource is set, so setURIList() refusing to
// promote file:// lines is the only defence under test.
static void testDragAndDropUntrustedURIListGrantsNoFiles(DragAndDropTest* test, gconstpointer)
{
    auto path = test->createFile("secret.txt", "sensitive contents");
    auto uriList = DragAndDropTest::uriForPath(path);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::UntrustedURIList);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "");
}

// A partial match must not become a partial grant.
static void testDragAndDropUntrustedMultipleURIsGrantNoFiles(DragAndDropTest* test, gconstpointer)
{
    auto real = test->createFile("real.txt", "real contents");
    auto uriList = makeString(DragAndDropTest::uriForPath(real), "\r\n"_s, "file:///etc/passwd\r\n"_s, "file:///nonexistent/path.txt\r\n"_s);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::UntrustedURIList);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "");
}

// The drop is built through the trusted path, so IsSource alone keeps the file out.
static void testDragAndDropSameAppDragIsDeniedFileAccess(DragAndDropTest* test, gconstpointer)
{
    auto path = test->createFile("secret.txt", "sensitive contents");
    auto uriList = DragAndDropTest::uriForPath(path);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::WebOrSameApp);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "");
}

// IsSource wins over a portal grant.
static void testDragAndDropSameAppDragIsDeniedEvenWithPortalList(DragAndDropTest* test, gconstpointer)
{
    auto path = test->createFile("portal.txt", "portal contents");
    auto uriList = DragAndDropTest::uriForPath(path);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { path }, WebViewTest::DropSource::WebOrSameApp);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "");
}

// The file manager drop the workaround broke.
static void testDragAndDropExternalFileDropGrantsFiles(DragAndDropTest* test, gconstpointer)
{
    auto path = test->createFile("dropped.txt", "hello from the file manager");
    auto uriList = DragAndDropTest::uriForPath(path);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 1);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileNames").utf8().data(), ==, "dropped.txt");
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "hello from the file manager");
}

// Order must match the uri-list.
static void testDragAndDropExternalMultipleFilesGrantAll(DragAndDropTest* test, gconstpointer)
{
    auto first = test->createFile("first.txt", "one");
    auto second = test->createFile("second.txt", "two");
    auto uriList = makeString(DragAndDropTest::uriForPath(first), "\r\n"_s, DragAndDropTest::uriForPath(second), "\r\n"_s);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 2);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileNames").utf8().data(), ==, "first.txt,second.txt");
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "one|two");
}

// The portal list is the grant. A parallel uri-list must not widen it.
static void testDragAndDropPortalListIsNotWidenedByURIList(DragAndDropTest* test, gconstpointer)
{
    auto granted = test->createFile("granted.txt", "granted");
    auto sneaked = test->createFile("sneaked.txt", "sneaked");
    auto uriList = makeString(DragAndDropTest::uriForPath(granted), "\r\n"_s, DragAndDropTest::uriForPath(sneaked));

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { granted }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 1);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileNames").utf8().data(), ==, "granted.txt");
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "granted");
}

// Replacement, not intersection.
static void testDragAndDropPortalListIsNotReplacedByURIList(DragAndDropTest* test, gconstpointer)
{
    auto granted = test->createFile("granted.txt", "granted");
    auto other = test->createFile("other.txt", "other");
    auto uriList = DragAndDropTest::uriForPath(other);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { granted }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 1);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileNames").utf8().data(), ==, "granted.txt");
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileContents").utf8().data(), ==, "granted");
}

// A drop that carries no file:// at all must not manufacture one.
static void testDragAndDropNonFileURIListGrantsNoFiles(DragAndDropTest* test, gconstpointer)
{
    test->loadDropTarget();
    test->dropFiles("https://webkit.org/\r\n", { }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
}

// A file URI naming another host is not a grant even on the trusted path.
static void testDragAndDropRemoteFileURIGrantsNoFiles(DragAndDropTest* test, gconstpointer)
{
    test->loadDropTarget();
    test->dropFiles("file://attacker.example/etc/passwd\r\n", { }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 0);
}

// Comments and blank lines are part of the text/uri-list format.
static void testDragAndDropURIListCommentsAreIgnored(DragAndDropTest* test, gconstpointer)
{
    auto path = test->createFile("kept.txt", "kept");
    auto uriList = makeString("# file:///etc/passwd\r\n"_s, "\r\n"_s, DragAndDropTest::uriForPath(path), "\r\n"_s);

    test->loadDropTarget();
    test->dropFiles(uriList.utf8().data(), { }, WebViewTest::DropSource::External);
    test->waitUntilDropHandled();

    g_assert_cmpint(test->droppedFileCount(), ==, 1);
    g_assert_cmpstr(test->stringByEvaluatingJavaScript("window.dropFileNames").utf8().data(), ==, "kept.txt");
}

void beforeAll()
{
    DragAndDropTest::add("DragAndDrop", "untrusted-uri-list-grants-no-files", testDragAndDropUntrustedURIListGrantsNoFiles);
    DragAndDropTest::add("DragAndDrop", "untrusted-multiple-uris-grant-no-files", testDragAndDropUntrustedMultipleURIsGrantNoFiles);
    DragAndDropTest::add("DragAndDrop", "same-app-drag-is-denied-file-access", testDragAndDropSameAppDragIsDeniedFileAccess);
    DragAndDropTest::add("DragAndDrop", "same-app-drag-is-denied-even-with-portal-list", testDragAndDropSameAppDragIsDeniedEvenWithPortalList);
    DragAndDropTest::add("DragAndDrop", "external-file-drop-grants-files", testDragAndDropExternalFileDropGrantsFiles);
    DragAndDropTest::add("DragAndDrop", "external-multiple-files-grant-all", testDragAndDropExternalMultipleFilesGrantAll);
    DragAndDropTest::add("DragAndDrop", "portal-list-is-not-widened-by-uri-list", testDragAndDropPortalListIsNotWidenedByURIList);
    DragAndDropTest::add("DragAndDrop", "portal-list-is-not-replaced-by-uri-list", testDragAndDropPortalListIsNotReplacedByURIList);
    DragAndDropTest::add("DragAndDrop", "non-file-uri-list-grants-no-files", testDragAndDropNonFileURIListGrantsNoFiles);
    DragAndDropTest::add("DragAndDrop", "remote-file-uri-grants-no-files", testDragAndDropRemoteFileURIGrantsNoFiles);
    DragAndDropTest::add("DragAndDrop", "uri-list-comments-are-ignored", testDragAndDropURIListCommentsAreIgnored);
}

void afterAll()
{
}

/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ChromiumBridge_h
#define ChromiumBridge_h

#include "FileSystem.h"
#include "ImageSource.h"
#include "LinkHash.h"
#include "PassRefPtr.h"
#include "PasteboardPrivate.h"

#include <wtf/Vector.h>

typedef struct NPObject NPObject;
typedef struct _NPP NPP_t;
typedef NPP_t* NPP;

#if OS(WINDOWS)
typedef struct HFONT__* HFONT;
#endif

namespace WebCore {

    class Color;
    class Cursor;
    class Document;
    class Frame;
    class GraphicsContext;
    class Image;
    class IntRect;
    class KURL;
    class String;
    class Widget;

    struct Cookie;
    struct PluginInfo;

    // An interface to the embedding layer, which has the ability to answer
    // questions about the system and so on...

    class ChromiumBridge {
    public:
        // Clipboard ----------------------------------------------------------
        static bool clipboardIsFormatAvailable(PasteboardPrivate::ClipboardFormat, PasteboardPrivate::ClipboardBuffer);

        static String clipboardReadPlainText(PasteboardPrivate::ClipboardBuffer);
        static void clipboardReadHTML(PasteboardPrivate::ClipboardBuffer, String*, KURL*);

        // Only the clipboardRead functions take a buffer argument because 
        // Chromium currently uses a different technique to write to alternate
        // clipboard buffers.
        static void clipboardWriteSelection(const String&, const KURL&, const String&, bool);
        static void clipboardWritePlainText(const String&);
        static void clipboardWriteURL(const KURL&, const String&);
        static void clipboardWriteImage(NativeImagePtr, const KURL&, const String&);

        // Cookies ------------------------------------------------------------
        static void setCookies(const KURL& url, const KURL& firstPartyForCookies, const String& value);
        static String cookies(const KURL& url, const KURL& firstPartyForCookies);
        static String cookieRequestHeaderFieldValue(const KURL& url, const KURL& firstPartyForCookies);
        static bool rawCookies(const KURL& url, const KURL& firstPartyForCookies, Vector<Cookie>*);
        static void deleteCookie(const KURL& url, const String& cookieName);
        static bool cookiesEnabled(const KURL& url, const KURL& firstPartyForCookies);

        // DNS ----------------------------------------------------------------
        static void prefetchDNS(const String& hostname);

        // File ---------------------------------------------------------------
        static bool fileExists(const String&);
        static bool deleteFile(const String&);
        static bool deleteEmptyDirectory(const String&);
        static bool getFileSize(const String&, long long& result);
        static bool getFileModificationTime(const String&, time_t& result);
        static String directoryName(const String& path);
        static String pathByAppendingComponent(const String& path, const String& component);
        static bool makeAllDirectories(const String& path);
        static String getAbsolutePath(const String&);
        static bool isDirectory(const String&);
        static KURL filePathToURL(const String&);

        // Font ---------------------------------------------------------------
#if OS(WINDOWS)
        static bool ensureFontLoaded(HFONT font);
#endif
#if OS(LINUX)
        static String getFontFamilyForCharacters(const UChar*, size_t numCharacters);
#endif

        // Forms --------------------------------------------------------------
        static void notifyFormStateChanged(const Document*);

        // HTML5 DB -----------------------------------------------------------
#if ENABLE(DATABASE)
        // Returns a handle to the DB file and ooptionally a handle to its containing directory
        static PlatformFileHandle databaseOpenFile(const String& vfsFleName, int desiredFlags, PlatformFileHandle* dirHandle = 0);
        // Returns a SQLite code (SQLITE_OK = 0, on success)
        static int databaseDeleteFile(const String& vfsFileName, bool syncDir = false);
        // Returns the attributes of the DB file
        static long databaseGetFileAttributes(const String& vfsFileName);
        // Returns the size of the DB file
        static long long databaseGetFileSize(const String& vfsFileName);
#endif

        // JavaScript ---------------------------------------------------------
        static void notifyJSOutOfMemory(Frame*);
        static bool allowScriptDespiteSettings(const KURL& documentURL);

        // Keygen -------------------------------------------------------------
        static String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String& challenge, const KURL& url);

        // Language -----------------------------------------------------------
        static String computedDefaultLanguage();

        // LayoutTestMode -----------------------------------------------------
        static bool layoutTestMode();

        // Memory -------------------------------------------------------------
        // Returns the current space allocated for the pagefile, in MB.
        // That is committed size for Windows and virtual memory size for POSIX
        static int memoryUsageMB();

        // MimeType -----------------------------------------------------------
        static bool isSupportedImageMIMEType(const String& mimeType);
        static bool isSupportedJavaScriptMIMEType(const String& mimeType);
        static bool isSupportedNonImageMIMEType(const String& mimeType);
        static String mimeTypeForExtension(const String& fileExtension);
        static String mimeTypeFromFile(const String& filePath);
        static String preferredExtensionForMIMEType(const String& mimeType);

        // Plugin -------------------------------------------------------------
        static bool plugins(bool refresh, Vector<PluginInfo*>*);
        static NPObject* pluginScriptableObject(Widget*);
        static bool popupsAllowed(NPP);

        // Resources ----------------------------------------------------------
        static PassRefPtr<Image> loadPlatformImageResource(const char* name);

        // Sandbox ------------------------------------------------------------
        static bool sandboxEnabled();

        // Screen -------------------------------------------------------------
        static int screenDepth(Widget*);
        static int screenDepthPerComponent(Widget*);
        static bool screenIsMonochrome(Widget*);
        static IntRect screenRect(Widget*);
        static IntRect screenAvailableRect(Widget*);

        // SharedTimers -------------------------------------------------------
        static void setSharedTimerFiredFunction(void (*func)());
        static void setSharedTimerFireTime(double fireTime);
        static void stopSharedTimer();

        // StatsCounters ------------------------------------------------------
        static void decrementStatsCounter(const char* name);
        static void incrementStatsCounter(const char* name);

        // Sudden Termination
        static void suddenTerminationChanged(bool enabled);

        // SystemTime ---------------------------------------------------------
        static double currentTime();

        // Theming ------------------------------------------------------------
#if OS(WINDOWS)
        static void paintButton(
            GraphicsContext*, int part, int state, int classicState, const IntRect&);
        static void paintMenuList(
            GraphicsContext*, int part, int state, int classicState, const IntRect&);
        static void paintScrollbarArrow(
            GraphicsContext*, int state, int classicState, const IntRect&);
        static void paintScrollbarThumb(
            GraphicsContext*, int part, int state, int classicState, const IntRect&);
        static void paintScrollbarTrack(
            GraphicsContext*, int part, int state, int classicState, const IntRect&, const IntRect& alignRect);
        static void paintTextField(
            GraphicsContext*, int part, int state, int classicState, const IntRect&, const Color&, bool fillContentArea, bool drawEdges);
        static void paintTrackbar(
            GraphicsContext*, int part, int state, int classicState, const IntRect&);
#endif

        // Trace Event --------------------------------------------------------
        static void traceEventBegin(const char* name, void* id, const char* extra);
        static void traceEventEnd(const char* name, void* id, const char* extra);

        // Visited links ------------------------------------------------------
        static LinkHash visitedLinkHash(const UChar* url, unsigned length);
        static LinkHash visitedLinkHash(const KURL& base, const AtomicString& attributeURL);
        static bool isLinkVisited(LinkHash);

        // Widget -------------------------------------------------------------
        static void widgetSetCursor(Widget*, const Cursor&);
        static void widgetSetFocus(Widget*);
    };

} // namespace WebCore

#endif

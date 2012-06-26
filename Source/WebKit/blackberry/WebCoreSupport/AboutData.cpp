/*
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "AboutData.h"

#include "CString.h"
#include "JSDOMWindow.h"
#include "MemoryCache.h"
#include "MemoryStatistics.h"
#include "SurfacePool.h"
#include "WebKitVersion.h"

#include <process.h>
#include <BlackBerryPlatformSettings.h>
#include <heap/Heap.h>
#include <runtime/JSGlobalData.h>
#include <sys/stat.h>
#include <sys/utsname.h>

namespace WebCore {

static String numberToHTMLTr(const String& description, unsigned number)
{
    return String("<tr><td>") + description + "</td><td>" + String::number(number) + "</td></tr>";
}

String configPage()
{
    String page;

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    page = String("<!DOCTYPE html><html><head><title>BlackBerry Browser Configuration Information</title><meta name=\"viewport\" content=\"width=700\">")
    + "<style>@media all and (orientation:landscape) { body { -webkit-column-count:2; -webkit-column-rule:solid; font-size:8px; } h1 { -webkit-column-span: all; } ul { font-size: 50%; } } td,li { text-overflow: ellipsis; overflow: hidden; } .list2 { width: 49%; float:left; padding-right:1px; } ul { list-style-type:none; padding:0px; margin:0px; } h1,h2,h3 { text-align:center; margin:0; } h2 { padding:1em 0 0 0; clear:both; } li,td { font-family:\"DejaVu Sans Condensed\"; }</style>"
    + "</head><body><h1>BlackBerry Browser Configuration Information</h1>"
    + "<h2>Compiler Information</h2><table>"
#if COMPILER(MSVC)
    + "<tr><td>Microsoft Visual C++</td><td>MSVC</td></tr>"
    + "<tr><td>_MSC_VER</td><td>" + String::number(_MSC_VER) + "</td></tr>"
    + "<tr><td>_MSC_FULL_VER</td><td>" + String::number(_MSC_FULL_VER) + "</td></tr>"
    + "<tr><td>_MSC_BUILD</td><td>" + String::number(_MSC_BUILD) + "</td></tr>"
#elif COMPILER(RVCT)
    + "<tr><td>ARM RealView Compiler Toolkit</td><td>RVCT</td></tr>"
    + "<tr><td>__ARMCC_VERSION</td><td>" + String::number(__ARMCC_VERSION) + "</td></tr>"
#if COMPILER(RVCT4GNU)
    + "<tr><td>RVCT 4+ in --gnu mode</td><td>1</td></tr>"
#endif
#elif COMPILER(GCC)
    + "<tr><td>GCC</td><td>" + String::number(__GNUC__) + "." + String::number(__GNUC_MINOR__) + "." + String::number(__GNUC_PATCHLEVEL__) + "</td></tr>"
#endif

    // Add "" to satisfy check-webkit-style.
    + "";

    page += String("</table><h2>CPU Information</h2><table>")
#if CPU(X86)
    + "<tr><td>X86</td><td></td></tr>"
#elif CPU(ARM)
    + "<tr><td>ARM</td><td></td></tr>"
    + "<tr><td>ARM_ARCH_VERSION</td><td>" + String::number(WTF_ARM_ARCH_VERSION) + "</td></tr>"
    + "<tr><td>THUMB_ARCH_VERSION</td><td>" + String::number(WTF_THUMB_ARCH_VERSION) + "</td></tr>"
    + "<tr><td>THUMB2</td><td>" + String::number(WTF_CPU_ARM_THUMB2) + "</td></tr>"
#endif
    + "<tr><td>Endianness</td><td>"
#if CPU(BIG_ENDIAN)
    + "big"
#elif CPU(MIDDLE_ENDIAN)
    + "middle"
#else
    + "little"
#endif
    + "</td></tr>";

    page += String("</table><h2>Platform Information</h2><table>")
    + "<tr><td>WebKit Version</td><td>" + String::number(WEBKIT_MAJOR_VERSION) + "." + String::number(WEBKIT_MINOR_VERSION) + "</td></tr>"
    + "<tr><td>BlackBerry</td><td>"
#if PLATFORM(BLACKBERRY)
    + "1"
#else
    + "0"
#endif
    + "</td></tr>"
    + "<tr><td>__STDC_ISO_10646__</td><td>"
#ifdef __STDC_ISO_10646__
    + "1"
#else
    + "0"
#endif
    + "</td></tr>";

    BlackBerry::Platform::Settings* settings = BlackBerry::Platform::Settings::instance();
    page += String("</table><h2>Platform Settings</h2><table>");
    page += numberToHTMLTr("isRSSFilteringEnabled", settings->isRSSFilteringEnabled());
    page += numberToHTMLTr("secondaryThreadStackSize", settings->secondaryThreadStackSize());
    page += numberToHTMLTr("maxPixelsPerDecodedImage", settings->maxPixelsPerDecodedImage());
    page += numberToHTMLTr("shouldReportLowMemoryToUser", settings->shouldReportLowMemoryToUser());
    page += numberToHTMLTr("numberOfBackingStoreTiles", settings->numberOfBackingStoreTiles());
    page += numberToHTMLTr("maximumNumberOfBackingStoreTilesAcrossProcesses", settings->maximumNumberOfBackingStoreTilesAcrossProcesses());
    page += numberToHTMLTr("tabsSupportedByClient", settings->tabsSupportedByClient());
    page += numberToHTMLTr("contextMenuEnabled", settings->contextMenuEnabled());
    page += numberToHTMLTr("selectionEnabled", settings->selectionEnabled());
    page += numberToHTMLTr("alwaysShowKeyboardOnFocus", settings->alwaysShowKeyboardOnFocus());
    page += numberToHTMLTr("allowCenterScrollAdjustmentForInputFields", settings->allowCenterScrollAdjustmentForInputFields());
    page += numberToHTMLTr("unrestrictedResizeEvents", settings->unrestrictedResizeEvents());
    page += numberToHTMLTr("isBridgeBrowser", settings->isBridgeBrowser());
    page += numberToHTMLTr("isWebGLSupported", settings->isWebGLSupported());
    page += numberToHTMLTr("showImageLocationOptionsInGCM", settings->showImageLocationOptionsInGCM());
    page += numberToHTMLTr("maxClickableSpeed", settings->maxClickableSpeed());
    page += numberToHTMLTr("maxJitterRadiusClick", settings->maxJitterRadiusClick());
    page += numberToHTMLTr("maxJitterRadiusTap", settings->maxJitterRadiusTap());
    page += numberToHTMLTr("maxJitterRadiusSingleTouchMove", settings->maxJitterRadiusSingleTouchMove());
    page += numberToHTMLTr("maxJitterRadiusTouchHold", settings->maxJitterRadiusTouchHold());
    page += numberToHTMLTr("maxJitterRadiusHandleDrag", settings->maxJitterRadiusHandleDrag());
    page += numberToHTMLTr("maxJitterRadiusTapHighlight", settings->maxJitterRadiusTapHighlight());
    page += numberToHTMLTr("maxJitterDistanceClick", settings->maxJitterDistanceClick());
    page += numberToHTMLTr("maxJitterDistanceTap", settings->maxJitterDistanceTap());
    page += numberToHTMLTr("maxJitterDistanceSingleTouchMove", settings->maxJitterDistanceSingleTouchMove());
    page += numberToHTMLTr("maxJitterDistanceTouchHold", settings->maxJitterDistanceTouchHold());
    page += numberToHTMLTr("maxJitterDistanceHandleDrag", settings->maxJitterDistanceHandleDrag());
    page += numberToHTMLTr("maxJitterDistanceTapHighlight", settings->maxJitterDistanceTapHighlight());
    page += numberToHTMLTr("topFatFingerPadding", settings->topFatFingerPadding());
    page += numberToHTMLTr("rightFatFingerPadding", settings->rightFatFingerPadding());
    page += numberToHTMLTr("bottomFatFingerPadding", settings->bottomFatFingerPadding());
    page += numberToHTMLTr("leftFatFingerPadding", settings->leftFatFingerPadding());

#define FOR_EACH_TRUE_LIST() \
    for (unsigned int i = 0; i < trueList.size(); ++i) \
        page += String("<li>") + trueList[i] + "</li>"

#define FOR_EACH_FALSE_LIST() \
    for (unsigned int i = 0; i < falseList.size(); ++i) \
        page += String("<li>") + falseList[i] + "</li>"

    Vector<String> trueList, falseList;
    #include "AboutDataEnableFeatures.cpp"
    page += String("</table><h2>WebKit ENABLE Information</h2><div class=\"list2\">");
    page += String("<h3>ENABLE</h3><ul>");
    FOR_EACH_TRUE_LIST();
    page += String("</ul></div><div class=\"list2\"><h3>don't ENABLE</h3><ul>");
    FOR_EACH_FALSE_LIST();
    page += String("</ul></div>");

    trueList.clear();
    falseList.clear();
    #include "AboutDataHaveFeatures.cpp"
    page += String("</table><h2>WebKit HAVE Information</h2><div class=\"list2\">");
    page += String("<h3>HAVE</h3><ul>");
    FOR_EACH_TRUE_LIST();
    page += String("</ul></div><div class=\"list2\"><h3>don't HAVE</h3><ul>");
    FOR_EACH_FALSE_LIST();
    page += String("</ul></div>");

    trueList.clear();
    falseList.clear();
    #include "AboutDataUseFeatures.cpp"
    page += String("<h2>WebKit USE Information</h2><div class=\"list2\">");
    page += String("<h3>USE</h3><ul>");
    FOR_EACH_TRUE_LIST();
    page += String("</ul></div><div class=\"list2\"><h3>don't USE</h3><ul>");
    FOR_EACH_FALSE_LIST();
    page += String("</ul></div>");

    page += String("</body></html>");
#endif

    return page;
}

static String cacheTypeStatisticToHTMLTr(const String& description, const MemoryCache::TypeStatistic& statistic)
{
    const int s_kiloByte = 1024;
    return String("<tr><td>") + description + "</td>"
        + "<td>" + String::number(statistic.count) + "</td>"
        + "<td>" + String::number(statistic.size / s_kiloByte) + "</td>"
        + "<td>" + String::number(statistic.liveSize / s_kiloByte) + "</td>"
        + "<td>" + String::number(statistic.decodedSize / s_kiloByte) + "</td>"
        + "</tr>";
}

static void dumpJSCTypeCountSetToTableHTML(String& tableHTML, JSC::TypeCountSet* typeCountSet)
{
    if (!typeCountSet)
        return;

    for (JSC::TypeCountSet::const_iterator iter = typeCountSet->begin(); iter != typeCountSet->end(); ++iter)
        tableHTML += numberToHTMLTr(iter->first, iter->second);
}

String memoryPage()
{
    String page;

    // generate memory information
    page = "<html><head><title>BlackBerry Browser Memory Information</title></head><body><h2>BlackBerry Browser Memory Information</h2>";

    // generate cache information
    MemoryCache* cacheInc = memoryCache();
    MemoryCache::Statistics cacheStat = cacheInc->getStatistics();

    page += "<h2>Cache Information</h2>"
            "<table align=\"center\" rules=\"all\"><tr> <th>Item</th> <th>Count</th> <th>Size<br>KB</th> <th>Living<br>KB</th> <th>Decoded<br>KB</th></tr>";

    MemoryCache::TypeStatistic total;
    total.count = cacheStat.images.count + cacheStat.cssStyleSheets.count
            + cacheStat.scripts.count + cacheStat.xslStyleSheets.count + cacheStat.fonts.count;
    total.size = cacheInc->totalSize();
    total.liveSize = cacheStat.images.liveSize + cacheStat.cssStyleSheets.liveSize
            + cacheStat.scripts.liveSize + cacheStat.xslStyleSheets.liveSize + cacheStat.fonts.liveSize;
    total.decodedSize = cacheStat.images.decodedSize
            + cacheStat.cssStyleSheets.decodedSize + cacheStat.scripts.decodedSize
            + cacheStat.xslStyleSheets.decodedSize + cacheStat.fonts.decodedSize;

    page += cacheTypeStatisticToHTMLTr("Total", total);
    page += cacheTypeStatisticToHTMLTr("Images", cacheStat.images);
    page += cacheTypeStatisticToHTMLTr("CSS Style Sheets", cacheStat.cssStyleSheets);
    page += cacheTypeStatisticToHTMLTr("Scripts", cacheStat.scripts);
#if ENABLE(XSLT)
    page += cacheTypeStatisticToHTMLTr("XSL Style Sheets", cacheStat.xslStyleSheets);
#endif
    page += cacheTypeStatisticToHTMLTr("Fonts", cacheStat.fonts);

    page += "</table>";

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD

    // JS engine memory usage.
    JSC::GlobalMemoryStatistics jscMemoryStat = JSC::globalMemoryStatistics();
    JSC::Heap& mainHeap = JSDOMWindow::commonJSGlobalData()->heap;
    OwnPtr<JSC::TypeCountSet> objectTypeCounts = mainHeap.objectTypeCounts();
    OwnPtr<JSC::TypeCountSet> protectedObjectTypeCounts = mainHeap.protectedObjectTypeCounts();

    page += "<h2>JS engine memory usage</h2><table align=\"center\" rules=\"all\">";

    page += numberToHTMLTr("Stack size", jscMemoryStat.stackBytes);
    page += numberToHTMLTr("JIT memory usage", jscMemoryStat.JITBytes);
    page += numberToHTMLTr("Main heap capacity", mainHeap.capacity());
    page += numberToHTMLTr("Main heap size", mainHeap.size());
    page += numberToHTMLTr("Object count", mainHeap.objectCount());
    page += numberToHTMLTr("Global object count", mainHeap.globalObjectCount());
    page += numberToHTMLTr("Protected object count", mainHeap.protectedObjectCount());
    page += numberToHTMLTr("Protected global object count", mainHeap.protectedGlobalObjectCount());

    page += "</table>";

    page += "<h3>Object type counts</h3><table align=\"center\" rules=\"all\">";
    dumpJSCTypeCountSetToTableHTML(page, objectTypeCounts.get());
    page += "</table>";

    page += "<h3>Protected object type counts</h3><table align=\"center\" rules=\"all\">";
    dumpJSCTypeCountSetToTableHTML(page, protectedObjectTypeCounts.get());
    page += "</table>";

    // Malloc info.
    struct mallinfo mallocInfo = mallinfo();

    page += "<h2>Malloc Information</h2><table align=\"center\" rules=\"all\">";

    page += numberToHTMLTr("Total space in use", mallocInfo.usmblks + mallocInfo.uordblks);
    page += numberToHTMLTr("Total space in free blocks", mallocInfo.fsmblks + mallocInfo.fordblks);
    page += numberToHTMLTr("Size of the arena", mallocInfo.arena);
    page += numberToHTMLTr("Number of big blocks in use", mallocInfo.ordblks);
    page += numberToHTMLTr("Number of small blocks in use", mallocInfo.smblks);
    page += numberToHTMLTr("Number of header blocks in use", mallocInfo.hblks);
    page += numberToHTMLTr("Space in header block headers", mallocInfo.hblkhd);
    page += numberToHTMLTr("Space in small blocks in use", mallocInfo.usmblks);
    page += numberToHTMLTr("Memory in free small blocks", mallocInfo.fsmblks);
    page += numberToHTMLTr("Space in big blocks in use", mallocInfo.uordblks);
    page += numberToHTMLTr("Memory in free big blocks", mallocInfo.fordblks);

    struct stat processInfo;
    if (!stat(String::format("/proc/%u/as", getpid()).latin1().data(), &processInfo))
        page += numberToHTMLTr("Process total mapped memory", processInfo.st_size);

    page += "</table>";
#endif

    page += "</body></html>";
    return page;
}

}

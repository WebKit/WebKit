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

#include "AboutTemplate.html.cpp"
#include "CString.h"
#include "CookieManager.h"
#include "JSDOMWindow.h"
#include "MemoryCache.h"
#include "MemoryStatistics.h"
#include "SurfacePool.h"
#include "WebKitVersion.h"

#include <BlackBerryPlatformClient.h>
#include <BlackBerryPlatformLog.h>
#include <BlackBerryPlatformMemory.h>
#include <BlackBerryPlatformSettings.h>
#include <BlackBerryPlatformWebKitCredits.h>
#include <BuildInformation.h>
#include <heap/Heap.h>
#include <process.h>
#include <runtime/JSGlobalData.h>
#include <sys/stat.h>
#include <sys/utsname.h>

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

static String writeFeatures(const Vector<String>& trueList, const Vector<String>& falseList)
{
    String ret;
    for (unsigned int i = 0, j = 0; i < trueList.size() || j < falseList.size();) {
        bool pickFromFalse = ((i >= trueList.size()) || (j < falseList.size() && strcmp(falseList[j].ascii().data(), trueList[i].ascii().data()) < 0));
        const String& item = (pickFromFalse ? falseList : trueList)[ (pickFromFalse ? j : i)++ ];
        ret += String("<tr><td><div class='" + String(pickFromFalse ? "false" : "true") + "'" + (item.length() >= 30 ? " style='font-size:10px;' " : "") + ">" + item + "</div></td></tr>");
    }
    return ret;
}

template<class T> static String numberToHTMLTr(const String& description, T number)
{
    return String("<tr><td>") + description + "</td><td>" + String::number(number) + "</td></tr>";
}

template<> String numberToHTMLTr<bool>(const String& description, bool truth)
{
    return String("<tr><td>") + description + "</td><td>" + (truth?"true":"false") + "</td></tr>";
}

static String configPage()
{
    String page;
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    page = writeHeader("Configuration")
    + "<div class=\"box\"><div class=\"box-title\">Compiler Information</div><table class='fixed-table'><col width=75%><col width=25%>"
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

    page += String("</table></div><br><div class='box'><div class='box-title'>CPU Information</div><table class='fixed-table'><col width=75%><col width=25%>")
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

    page += String("</table></div><br><div class='box'><div class='box-title'>Platform Information</div><table class='fixed-table'><col width=75%><col width=25%>")
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
    page += String("</table></div><br><div class='box'><div class='box-title'>Platform Settings</div><table style='font-size:11px;' class='fixed-table'><col width=75%><col width=25%>");
    page += numberToHTMLTr("isRSSFilteringEnabled", settings->isRSSFilteringEnabled());
    page += numberToHTMLTr("secondaryThreadStackSize", settings->secondaryThreadStackSize());
    page += numberToHTMLTr("maxPixelsPerDecodedImage", settings->maxPixelsPerDecodedImage());
    page += numberToHTMLTr("shouldReportLowMemoryToUser", settings->shouldReportLowMemoryToUser());
    page += numberToHTMLTr("numberOfBackingStoreTiles", settings->numberOfBackingStoreTiles());
    page += numberToHTMLTr("maximumNumberOfBacking...AcrossProcesses", settings->maximumNumberOfBackingStoreTilesAcrossProcesses());
    page += numberToHTMLTr("tabsSupportedByClient", settings->tabsSupportedByClient());
    page += numberToHTMLTr("contextMenuEnabled", settings->contextMenuEnabled());
    page += numberToHTMLTr("selectionEnabled", settings->selectionEnabled());
    page += numberToHTMLTr("fineCursorControlEnabled", settings->fineCursorControlEnabled());
    page += numberToHTMLTr("alwaysShowKeyboardOnFocus", settings->alwaysShowKeyboardOnFocus());
    page += numberToHTMLTr("allowCenterScrollAdjustmentForInputFields", settings->allowCenterScrollAdjustmentForInputFields());
    page += numberToHTMLTr("unrestrictedResizeEvents", settings->unrestrictedResizeEvents());
    page += numberToHTMLTr("isBridgeBrowser", settings->isBridgeBrowser());
    page += numberToHTMLTr("isWebGLSupported", settings->isWebGLSupported());
    page += numberToHTMLTr("showImageLocationOptionsInGCM", settings->showImageLocationOptionsInGCM());
    page += numberToHTMLTr("forceGLES2WindowUsage", settings->forceGLES2WindowUsage());
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
    page += numberToHTMLTr("maxJitterDistanceTapHighlight", settings->maxJitterDistanceTapHighlight());
    page += numberToHTMLTr("maxJitterDistanceHandleDrag", settings->maxJitterDistanceHandleDrag());
    page += numberToHTMLTr("topFatFingerPadding", settings->topFatFingerPadding());
    page += numberToHTMLTr("rightFatFingerPadding", settings->rightFatFingerPadding());
    page += numberToHTMLTr("bottomFatFingerPadding", settings->bottomFatFingerPadding());
    page += numberToHTMLTr("maxSelectionNeckHeight", settings->maxSelectionNeckHeight());
    page += numberToHTMLTr("leftFatFingerPadding", settings->leftFatFingerPadding());

    Vector<String> trueList, falseList;
    #include "AboutDataEnableFeatures.cpp"
    page += String("</table></div><br><div class='box'><div class='box-title'>WebKit Features (ENABLE_)</div><table class='fixed-table'>");

    page += writeFeatures(trueList, falseList);

    trueList.clear();
    falseList.clear();
    #include "AboutDataHaveFeatures.cpp"
    page += String("</table></div><br><div class='box'><div class='box-title'>WebKit Features (HAVE_)</div><table class='fixed-table'>");

    page += writeFeatures(trueList, falseList);

    trueList.clear();
    falseList.clear();
    #include "AboutDataUseFeatures.cpp"
    page += String("</table></div><br><div class='box'><div class='box-title'>WebKit Features (USE_)</div><table class='fixed-table'>");
    page += writeFeatures(trueList, falseList);
    page += String("</table></div></body></html>");
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

static String memoryPage()
{
    String page;

    page = writeHeader("Memory")
    + "<div class=\"box\"><div class=\"box-title\">Cache Information<br><div style='font-size:11px;color:#A8A8A8'>Size, Living, and Decoded are expressed in KB.</div><br></div><table class='fixed-table'><col width=75%><col width=25%>";

    // generate cache information
    MemoryCache* cacheInc = memoryCache();
    MemoryCache::Statistics cacheStat = cacheInc->getStatistics();

    page += "<tr> <th align=left>Item</th> <th align=left>Count</th> <th align=left>Size</th> <th align=left>Living</th> <th align=left>Decoded</th></tr>";

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

    page += "</table></div><br>";

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD

    // JS engine memory usage.
    JSC::GlobalMemoryStatistics jscMemoryStat = JSC::globalMemoryStatistics();
    JSC::Heap& mainHeap = JSDOMWindow::commonJSGlobalData()->heap;
    OwnPtr<JSC::TypeCountSet> objectTypeCounts = mainHeap.objectTypeCounts();
    OwnPtr<JSC::TypeCountSet> protectedObjectTypeCounts = mainHeap.protectedObjectTypeCounts();

    // Malloc info.
    struct mallinfo mallocInfo = mallinfo();

    page += "<div class='box'><div class='box-title'>Process memory usage summary</div><table class='fixed-table'><col width=75%><col width=25%>";

    page += numberToHTMLTr("Total used memory (malloc + JSC)", mallocInfo.usmblks + mallocInfo.uordblks + jscMemoryStat.stackBytes + jscMemoryStat.JITBytes + mainHeap.capacity());

    if (unsigned totalCommittedMemoryOfChromeProcess = BlackBerry::Platform::totalCommittedMemoryOfChromeProcess()) {
        page += numberToHTMLTr("Total committed memory of tab process", BlackBerry::Platform::totalCommittedMemoryOfCurrentProcess());
        page += numberToHTMLTr("Total committed memory of chrome process", totalCommittedMemoryOfChromeProcess);
    } else
        page += numberToHTMLTr("Total committed memory", BlackBerry::Platform::totalCommittedMemoryOfCurrentProcess());

    struct stat processInfo;
    if (!stat(String::format("/proc/%u/as", getpid()).latin1().data(), &processInfo))
        page += numberToHTMLTr("Total mapped memory", processInfo.st_size);

    page += numberToHTMLTr("System free memory", BlackBerry::Platform::systemFreeMemory());

    page += "</table></div><br>";

    page += "<div class='box'><div class='box-title'>JS engine memory usage</div><table class='fixed-table'><col width=75%><col width=25%>";

    page += numberToHTMLTr("Stack size", jscMemoryStat.stackBytes);
    page += numberToHTMLTr("JIT memory usage", jscMemoryStat.JITBytes);
    page += numberToHTMLTr("Main heap capacity", mainHeap.capacity());
    page += numberToHTMLTr("Main heap size", mainHeap.size());
    page += numberToHTMLTr("Object count", mainHeap.objectCount());
    page += numberToHTMLTr("Global object count", mainHeap.globalObjectCount());
    page += numberToHTMLTr("Protected object count", mainHeap.protectedObjectCount());
    page += numberToHTMLTr("Protected global object count", mainHeap.protectedGlobalObjectCount());

    page += "</table></div><br>";

    page += "<div class='box'><div class='box-title'>JS object type counts</div><table class='fixed-table'><col width=75%><col width=25%>";
    dumpJSCTypeCountSetToTableHTML(page, objectTypeCounts.get());
    page += "</table></div><br>";

    page += "<div class='box'><div class='box-title'>JS protected object type counts</div><table class='fixed-table'><col width=75%><col width=25%>";
    dumpJSCTypeCountSetToTableHTML(page, protectedObjectTypeCounts.get());
    page += "</table></div><br>";

    page += "<div class='box'><div class='box-title'>Malloc Information</div><table class='fixed-table'><col width=75%><col width=25%>";

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

    page += "</table></div>";
#endif

    page += "</body></html>";
    return page;
}

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
class MemoryTracker {
public:
    static MemoryTracker& instance();
    void start();
    void stop();
    bool isActive() const { return m_memoryTrackingTimer.isActive(); }
    void clear()
    {
        m_peakTotalUsedMemory = 0;
        m_peakTotalCommittedMemoryOfCurrentProcess = 0;
        m_peakTotalCommittedMemoryOfChromeProcess = 0;
        m_peakTotalMappedMemory = 0;
    }

    void updateMemoryPeaks(Timer<MemoryTracker>*);
    unsigned peakTotalUsedMemory() const { return m_peakTotalUsedMemory; }
    unsigned peakTotalCommittedMemoryOfCurrentProcess() const { return m_peakTotalCommittedMemoryOfCurrentProcess; }
    unsigned peakTotalCommittedMemoryOfChromeProcess() const { return m_peakTotalCommittedMemoryOfChromeProcess; }
    unsigned peakTotalMappedMemory() const { return m_peakTotalMappedMemory; }

private:
    MemoryTracker();
    Timer<MemoryTracker> m_memoryTrackingTimer;
    unsigned m_peakTotalUsedMemory;
    unsigned m_peakTotalCommittedMemoryOfCurrentProcess;
    unsigned m_peakTotalCommittedMemoryOfChromeProcess;
    unsigned m_peakTotalMappedMemory;
};

MemoryTracker::MemoryTracker()
    : m_memoryTrackingTimer(this, &MemoryTracker::updateMemoryPeaks)
    , m_peakTotalUsedMemory(0)
    , m_peakTotalCommittedMemoryOfCurrentProcess(0)
    , m_peakTotalCommittedMemoryOfChromeProcess(0)
    , m_peakTotalMappedMemory(0)
{
}

MemoryTracker& MemoryTracker::instance()
{
    DEFINE_STATIC_LOCAL(MemoryTracker, s_memoryTracker, ());
    return s_memoryTracker;
}

void MemoryTracker::start()
{
    clear();
    if (!m_memoryTrackingTimer.isActive())
        m_memoryTrackingTimer.start(0, 0.01);
}

void MemoryTracker::stop()
{
    m_memoryTrackingTimer.stop();
}

void MemoryTracker::updateMemoryPeaks(Timer<MemoryTracker>*)
{
    // JS engine memory usage.
    JSC::GlobalMemoryStatistics jscMemoryStat = JSC::globalMemoryStatistics();
    JSC::Heap& mainHeap = JSDOMWindow::commonJSGlobalData()->heap;

    // Malloc info.
    struct mallinfo mallocInfo = mallinfo();

    // Malloc and JSC memory.
    unsigned totalUsedMemory = static_cast<unsigned>(mallocInfo.usmblks + mallocInfo.uordblks + jscMemoryStat.stackBytes + jscMemoryStat.JITBytes + mainHeap.capacity());
    if (totalUsedMemory > m_peakTotalUsedMemory)
        m_peakTotalUsedMemory = totalUsedMemory;

    unsigned totalCommittedMemoryOfCurrentProcess = BlackBerry::Platform::totalCommittedMemoryOfCurrentProcess();
    if (totalCommittedMemoryOfCurrentProcess > m_peakTotalCommittedMemoryOfCurrentProcess)
        m_peakTotalCommittedMemoryOfCurrentProcess = totalCommittedMemoryOfCurrentProcess;

    unsigned totalCommittedMemoryOfChromeProcess = BlackBerry::Platform::totalCommittedMemoryOfChromeProcess();
    if (totalCommittedMemoryOfChromeProcess > m_peakTotalCommittedMemoryOfChromeProcess)
        m_peakTotalCommittedMemoryOfChromeProcess = totalCommittedMemoryOfChromeProcess;

    struct stat processInfo;
    if (!stat(String::format("/proc/%u/as", getpid()).latin1().data(), &processInfo)) {
        unsigned totalMappedMemory = static_cast<unsigned>(processInfo.st_size);
        if (totalMappedMemory > m_peakTotalMappedMemory)
            m_peakTotalMappedMemory = totalMappedMemory;
    }
}

static String memoryPeaksToHtmlTable(MemoryTracker& memoryTracker)
{
    String htmlTable = String("<table class='fixed-table'><col width=75%><col width=25%>")
        + numberToHTMLTr("Total used memory(malloc + JSC):", memoryTracker.peakTotalUsedMemory());

    if (unsigned peakTotalCommittedMemoryOfChromeProcess = memoryTracker.peakTotalCommittedMemoryOfChromeProcess()) {
        htmlTable += numberToHTMLTr("Total committed memory of tab process:", memoryTracker.peakTotalCommittedMemoryOfCurrentProcess());
        htmlTable += numberToHTMLTr("Total committed memory of chrome process:", peakTotalCommittedMemoryOfChromeProcess);
    } else
        htmlTable += numberToHTMLTr("Total committed memory:", memoryTracker.peakTotalCommittedMemoryOfCurrentProcess());

    htmlTable += numberToHTMLTr("Total mapped memory:", memoryTracker.peakTotalMappedMemory()) + "</table>";
    return htmlTable;
}

static String memoryLivePage(String memoryLiveCommand)
{
    String page;

    page = writeHeader("Memory Live Page")
        + "<div class='box'><div class='box-title'>Memory Peaks</div>"
        + "<div style='font-size:12px;color:#1BE0C9'>\"about:memory-live/start\": start tracking memory peaks.</div>"
        + "<div style='font-size:12px;color:#1BE0C9'>\"about:memory-live\": show memory peaks every 30ms.</div>"
        + "<div style='font-size:12px;color:#1BE0C9'>\"about:memory-live/stop\": stop tracking and show memory peaks.</div><br>";

    MemoryTracker& memoryTracker = MemoryTracker::instance();
    if (memoryLiveCommand.isEmpty()) {
        if (!memoryTracker.isActive())
            page += "<div style='font-size:15px;color:#E6F032'>Memory tracker isn't running, please use \"about:memory-live/start\" to start the tracker.</div>";
        else {
            page += memoryPeaksToHtmlTable(memoryTracker);
            page += "<script type=\"text/javascript\">setInterval(function(){window.location.reload()},30);</script>";
        }
    } else if (equalIgnoringCase(memoryLiveCommand, "/start")) {
        memoryTracker.start();
        page += "<div style='font-size:15px;color:#E6F032'>Memory tracker is running.</div>";
    } else if (equalIgnoringCase(memoryLiveCommand, "/stop")) {
        if (!memoryTracker.isActive())
            page += "<div style='font-size:15px;color:#E6F032'>Memory tracker isn't running.</div>";
        else {
            memoryTracker.stop();
            page += memoryPeaksToHtmlTable(memoryTracker);
            page += "<div style='font-size:15px;color:#E6F032'>Memory tracker is stopped.</div>";
        }
    } else
        page += "<div style='font-size:15spx;color:#E6F032'>Unknown command! Please input a correct command!</div>";

    page += "</div><br></div></body></html>";
    return page;
}
#endif

static String cachePage(String cacheCommand)
{
    String result;

    result.append(String("<html><head><title>BlackBerry Browser Disk Cache</title></head><body>"));

    BlackBerry::Platform::Client* client = BlackBerry::Platform::Client::get();
    ASSERT(client);

    if (cacheCommand.isEmpty())
        result.append(String(client->generateHtmlFragmentForCacheKeys().data()));
    else if (cacheCommand.startsWith("?query=", false)) {
        std::string key(cacheCommand.substring(7).utf8().data()); // 7 is length of "query=".
        result.append(String(key.data()));
        result.append(String("<hr>"));
        result.append(String(client->generateHtmlFragmentForCacheHeaders(key).data()));
    }
#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    else if (equalIgnoringCase(cacheCommand, "/disable")) {
        client->setDiskCacheEnabled(false);
        result.append("Http disk cache is disabled.");
    } else if (equalIgnoringCase(cacheCommand, "/enable")) {
        client->setDiskCacheEnabled(true);
        result.append("Http disk cache is enabled.");
    }
#endif
    else {
        // Unknown cache command.
        return String();
    }

    result.append(String("</body></html>"));

    return result;
}

static String buildPage()
{
    String result;

    result.append(writeHeader("Build"));
    result.append(String("<div class='box'><div class='box-title'>Basic</div><table>"));
    result.append(String("<tr><td>Built On:  </td><td>"));
    result.append(String(BlackBerry::Platform::BUILDCOMPUTER));
    result.append(String("</td></tr>"));
    result.append(String("<tr><td>Build User:  </td><td>"));
    result.append(String(BlackBerry::Platform::BUILDUSER));
    result.append(String("</td></tr>"));
    result.append(String("<tr><td>Build Time:  </td><td>"));
    result.append(String(BlackBerry::Platform::BUILDTIME));
    result.append(String("</table></div><br>"));
    result.append(String(BlackBerry::Platform::BUILDINFO_WEBKIT));
    result.append(String(BlackBerry::Platform::BUILDINFO_PLATFORM));
    result.append(String(BlackBerry::Platform::BUILDINFO_LIBWEBVIEW));
    result.append(String("</body></html>"));

    return result;
}

static String creditsPage()
{
    String result;

    result.append(writeHeader("Credits"));
    result.append(String("<style> .about {padding:14px;} </style>"));
    result.append(String(BlackBerry::Platform::WEBKITCREDITS));
    result.append(String("</body></html>"));

    return result;
}

static String cookiePage()
{
    String result;

    result.append(String("<html><head><title>BlackBerry Browser cookie information</title></head><body>"));
    result.append(cookieManager().generateHtmlFragmentForCookies());
    result.append(String("</body></html>"));

    return result;
}

String aboutData(String aboutWhat)
{
    if (equalIgnoringCase(aboutWhat, "credits"))
        return creditsPage();

    if (aboutWhat.startsWith("cache"))
        return cachePage(aboutWhat.substring(5));

    if (equalIgnoringCase(aboutWhat, "memory"))
        return memoryPage();

#if !defined(PUBLIC_BUILD) || !PUBLIC_BUILD
    if (equalIgnoringCase(aboutWhat, "cookie"))
        return cookiePage();

    if (BlackBerry::Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "build"))
        return buildPage();

    if (BlackBerry::Platform::debugSetting() > 0 && equalIgnoringCase(aboutWhat, "config"))
        return configPage();

    if (aboutWhat.startsWith("memory-live"))
        return memoryLivePage(aboutWhat.substring(11));
#endif

    return String();
}

} // namespace WebKit
} // namespace BlackBerry

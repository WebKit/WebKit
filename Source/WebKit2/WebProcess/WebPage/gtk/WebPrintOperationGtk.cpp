/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebPrintOperationGtk.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/IntRect.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformContextCairo.h>
#include <WebCore/PrintContext.h>
#include <gtk/gtk.h>
#include <wtf/Vector.h>
#include <wtf/gobject/GOwnPtr.h>

#ifdef HAVE_GTK_UNIX_PRINTING
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <gtk/gtkunixprint.h>
#endif

namespace WebKit {

#ifdef HAVE_GTK_UNIX_PRINTING
class WebPrintOperationGtkUnix: public WebPrintOperationGtk {
public:
    WebPrintOperationGtkUnix(WebPage* page, const PrintInfo& printInfo)
        : WebPrintOperationGtk(page, printInfo)
        , m_printJob(0)
    {
    }

    static gboolean enumeratePrintersFunction(GtkPrinter* printer, WebPrintOperationGtkUnix* printOperation)
    {
        GtkPrinter* selectedPrinter = 0;
        const char* printerName = gtk_print_settings_get_printer(printOperation->printSettings());
        if (printerName) {
            if (!strcmp(printerName, gtk_printer_get_name(printer)))
                selectedPrinter = printer;
        } else if (gtk_printer_is_default(printer))
            selectedPrinter = printer;

        if (!selectedPrinter)
            return FALSE;

        static int jobNumber = 0;
        const char* applicationName = g_get_application_name();
        GOwnPtr<char>jobName(g_strdup_printf("%s job #%d", applicationName ? applicationName : "WebKit", ++jobNumber));
        printOperation->m_printJob = adoptGRef(gtk_print_job_new(jobName.get(), selectedPrinter,
                                                                 printOperation->printSettings(),
                                                                 printOperation->pageSetup()));
        return TRUE;
    }

    static void enumeratePrintersFinished(WebPrintOperationGtkUnix* printOperation)
    {
        if (!printOperation->m_printJob) {
            // FIXME: Printing error.
            return;
        }

        cairo_surface_t* surface = gtk_print_job_get_surface(printOperation->m_printJob.get(), 0);
        if (!surface) {
            // FIXME: Printing error.
            return;
        }

        int rangesCount;
        printOperation->m_pageRanges = gtk_print_job_get_page_ranges(printOperation->m_printJob.get(), &rangesCount);
        printOperation->m_pageRangesCount = rangesCount;
        printOperation->m_pagesToPrint = gtk_print_job_get_pages(printOperation->m_printJob.get());
        printOperation->m_needsRotation = gtk_print_job_get_rotate(printOperation->m_printJob.get());

        printOperation->print(surface, 72, 72);
    }

    void startPrint(WebCore::PrintContext* printContext, uint64_t callbackID)
    {
        m_printContext = printContext;
        m_callbackID = callbackID;
        gtk_enumerate_printers(reinterpret_cast<GtkPrinterFunc>(enumeratePrintersFunction), this,
                               reinterpret_cast<GDestroyNotify>(enumeratePrintersFinished), FALSE);
    }

    void startPage(cairo_t* cr)
    {
        GtkPaperSize* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
        double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_POINTS);
        double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_POINTS);

        cairo_surface_t* surface = gtk_print_job_get_surface(m_printJob.get(), 0);
        cairo_surface_type_t surfaceType = cairo_surface_get_type(surface);
        if (surfaceType == CAIRO_SURFACE_TYPE_PS) {
            cairo_ps_surface_set_size(surface, width, height);
            cairo_ps_surface_dsc_begin_page_setup(surface);

            switch (gtk_page_setup_get_orientation(m_pageSetup.get())) {
            case GTK_PAGE_ORIENTATION_PORTRAIT:
            case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
                cairo_ps_surface_dsc_comment(surface, "%%PageOrientation: Portrait");
                break;
            case GTK_PAGE_ORIENTATION_LANDSCAPE:
            case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
                cairo_ps_surface_dsc_comment(surface, "%%PageOrientation: Landscape");
                break;
            }
        } else if (surfaceType == CAIRO_SURFACE_TYPE_PDF)
            cairo_pdf_surface_set_size(surface, width, height);
    }

    void endPage(cairo_t* cr)
    {
        cairo_show_page(cr);
    }

    static void printJobComplete(GtkPrintJob* printJob, WebPrintOperationGtkUnix* printOperation, const GError*)
    {
        printOperation->m_printJob = 0;
    }

    static void printJobFinished(WebPrintOperationGtkUnix* printOperation)
    {
        printOperation->deref();
    }

    void endPrint()
    {
        cairo_surface_finish(gtk_print_job_get_surface(m_printJob.get(), 0));
        // Make sure the operation is alive until the job is sent.
        ref();
        gtk_print_job_send(m_printJob.get(), reinterpret_cast<GtkPrintJobCompleteFunc>(printJobComplete), this,
                           reinterpret_cast<GDestroyNotify>(printJobFinished));
    }

    GRefPtr<GtkPrintJob> m_printJob;
};
#endif

#ifdef G_OS_WIN32
class WebPrintOperationGtkWin32: public WebPrintOperationGtk {
public:
    WebPrintOperationGtkWin32(WebPage* page, const PrintInfo& printInfo)
        : WebPrintOperationGtk(page, printInfo)
    {
    }

    void startPrint(WebCore::PrintContext* printContext, uint64_t callbackID)
    {
        m_printContext = printContext;
        m_callbackID = callbackID;
        notImplemented();
    }

    void startPage(cairo_t* cr)
    {
        notImplemented();
    }

    void endPage(cairo_t* cr)
    {
        notImplemented();
    }

    void endPrint()
    {
        notImplemented();
    }
};
#endif

struct PrintPagesData {
    PrintPagesData(WebPrintOperationGtk* printOperation)
        : printOperation(printOperation)
        , totalPrinted(-1)
        , pageNumber(0)
        , pagePosition(0)
        , isDone(false)
    {
        if (printOperation->pagesToPrint() == GTK_PRINT_PAGES_RANGES) {
            Vector<GtkPageRange> pageRanges;
            GtkPageRange* ranges = printOperation->pageRanges();
            size_t rangesCount = printOperation->pageRangesCount();
            int pageCount = printOperation->pageCount();

            pageRanges.reserveCapacity(rangesCount);
            for (size_t i = 0; i < rangesCount; ++i) {
                if (ranges[i].start >= 0 && ranges[i].start < pageCount && ranges[i].end >= 0 && ranges[i].end < pageCount)
                    pageRanges.append(ranges[i]);
                else if (ranges[i].start >= 0 && ranges[i].start < pageCount && ranges[i].end >= pageCount) {
                    pageRanges.append(ranges[i]);
                    pageRanges.last().end = pageCount - 1;
                } else if (ranges[i].end >= 0 && ranges[i].end < pageCount && ranges[i].start < 0) {
                    pageRanges.append(ranges[i]);
                    pageRanges.last().start = 0;
                }
            }

            for (size_t i = 0; i < pageRanges.size(); ++i) {
                for (int j = pageRanges[i].start; j <= pageRanges[i].end; ++j)
                    pages.append(j);
            }

        } else {
            for (int i = 0; i < printOperation->pageCount(); ++i)
                pages.append(i);
        }

        pageNumber = pages[pagePosition];
    }

    void incrementPageSequence()
    {
        if (totalPrinted == -1) {
            totalPrinted = 0;
            return;
        }

        pagePosition++;
        if (pagePosition >= pages.size()) {
            isDone = true;
            return;
        }
        pageNumber = pages[pagePosition];
        totalPrinted++;
    }

    RefPtr<WebPrintOperationGtk> printOperation;

    int totalPrinted;
    size_t totalToPrint;
    int pageNumber;
    size_t pagePosition;
    Vector<size_t> pages;

    bool isDone : 1;
};

PassRefPtr<WebPrintOperationGtk> WebPrintOperationGtk::create(WebPage* page, const PrintInfo& printInfo)
{
#ifdef HAVE_GTK_UNIX_PRINTING
    return adoptRef(new WebPrintOperationGtkUnix(page, printInfo));
#endif
#ifdef G_OS_WIN32
    return adoptRef(new WebPrintOperationGtkWin32(page, printInfo));
#endif
}

WebPrintOperationGtk::WebPrintOperationGtk(WebPage* page, const PrintInfo& printInfo)
    : m_webPage(page)
    , m_printSettings(printInfo.printSettings.get())
    , m_pageSetup(printInfo.pageSetup.get())
    , m_printContext(0)
    , m_callbackID(0)
    , m_xDPI(1)
    , m_yDPI(1)
    , m_printPagesIdleId(0)
    , m_pagesToPrint(GTK_PRINT_PAGES_ALL)
    , m_pageRanges(0)
    , m_pageRangesCount(0)
    , m_needsRotation(false)
{
}

WebPrintOperationGtk::~WebPrintOperationGtk()
{
    if (m_printPagesIdleId)
        g_source_remove(m_printPagesIdleId);
}

int WebPrintOperationGtk::pageCount() const
{
    return m_printContext ? m_printContext->pageCount() : 0;
}

void WebPrintOperationGtk::rotatePage()
{
    GtkPaperSize* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
    double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_INCH) * m_xDPI;
    double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_INCH) * m_yDPI;

    cairo_matrix_t matrix;
    switch (gtk_page_setup_get_orientation(m_pageSetup.get())) {
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
        cairo_translate(m_cairoContext.get(), 0, height);
        cairo_matrix_init(&matrix, 0, -1, 1, 0, 0, 0);
        cairo_transform(m_cairoContext.get(), &matrix);
        break;
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
        cairo_translate(m_cairoContext.get(), width, height);
        cairo_matrix_init(&matrix, -1, 0, 0, -1, 0, 0);
        cairo_transform(m_cairoContext.get(), &matrix);
        break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
        cairo_translate(m_cairoContext.get(), width, 0);
        cairo_matrix_init(&matrix, 0, 1, -1, 0, 0, 0);
        cairo_transform(m_cairoContext.get(), &matrix);
        break;
    case GTK_PAGE_ORIENTATION_PORTRAIT:
    default:
        break;
    }
}

void WebPrintOperationGtk::renderPage(int pageNumber)
{
    startPage(m_cairoContext.get());

    if (m_needsRotation)
        rotatePage();

    double left = gtk_page_setup_get_left_margin(m_pageSetup.get(), GTK_UNIT_INCH);
    double top = gtk_page_setup_get_top_margin(m_pageSetup.get(), GTK_UNIT_INCH);
    cairo_translate(m_cairoContext.get(), left * m_xDPI, top * m_yDPI);

    double pageWidth = gtk_page_setup_get_page_width(m_pageSetup.get(), GTK_UNIT_INCH) * m_xDPI;
    WebCore::PlatformContextCairo platformContext(m_cairoContext.get());
    WebCore::GraphicsContext graphicsContext(&platformContext);
    m_printContext->spoolPage(graphicsContext, pageNumber, pageWidth);

    endPage(m_cairoContext.get());
}

gboolean WebPrintOperationGtk::printPagesIdle(gpointer userData)
{
    PrintPagesData* data = static_cast<PrintPagesData*>(userData);

    data->incrementPageSequence();
    if (data->isDone)
        return FALSE;

    data->printOperation->renderPage(data->pageNumber);
    return TRUE;
}

void WebPrintOperationGtk::printPagesIdleDone(gpointer userData)
{
    PrintPagesData* data = static_cast<PrintPagesData*>(userData);

    data->printOperation->printDone();
    delete data;
}

void WebPrintOperationGtk::printDone()
{
    m_printPagesIdleId = 0;

    endPrint();
    // Job is now sent to the printer, we can notify the UI process that we are done.
    m_webPage->send(Messages::WebPageProxy::VoidCallback(m_callbackID));
    m_cairoContext = 0;
}

void WebPrintOperationGtk::print(cairo_surface_t* surface, double xDPI, double yDPI)
{
    ASSERT(m_printContext);

    m_xDPI = xDPI;
    m_yDPI = yDPI;
    m_cairoContext = adoptRef(cairo_create(surface));

    PrintPagesData* data = new PrintPagesData(this);
    m_printPagesIdleId = gdk_threads_add_idle_full(G_PRIORITY_DEFAULT_IDLE + 10, printPagesIdle,
                                                   data, printPagesIdleDone);
}

}

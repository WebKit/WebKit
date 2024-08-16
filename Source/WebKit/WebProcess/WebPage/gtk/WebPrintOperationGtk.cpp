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

#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/IntRect.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PrintContext.h>
#include <WebCore/ResourceError.h>
#include <gtk/gtk.h>
#include <memory>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(CAIRO)
#include <WebCore/GraphicsContextCairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <cairo-svg.h>
#elif USE(SKIA)
#include <WebCore/GraphicsContextSkia.h>
#include <skia/docs/SkPDFDocument.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPrintOperationGtk);

WebPrintOperationGtk::PrintPagesData::PrintPagesData(WebPrintOperationGtk* printOperation)
    : printOperation(printOperation)
{
    if (printOperation->m_printMode == PrintInfo::PrintMode::Sync)
        mainLoop = adoptGRef(g_main_loop_new(0, FALSE));

    if (printOperation->m_collateCopies) {
        collatedCopies = printOperation->m_copies;
        uncollatedCopies = 1;
    } else {
        collatedCopies = 1;
        uncollatedCopies = printOperation->m_copies;
    }

    if (printOperation->m_pagesToPrint == GTK_PRINT_PAGES_RANGES) {
        Vector<GtkPageRange> pageRanges;
        GtkPageRange* ranges = printOperation->m_pageRanges;
        size_t rangesCount = printOperation->m_pageRangesCount;
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

    if (!pages.size()) {
        isValid = false;
        return;
    }
    printOperation->m_numberOfPagesToPrint = pages.size();

    size_t numberUp = printOperation->m_numberUp;
    if (numberUp > 1)
        numberOfSheets = (pages.size() % numberUp) ? pages.size() / numberUp + 1 : pages.size() / numberUp;
    else
        numberOfSheets = pages.size();

    bool reverse = printOperation->m_reverse;
    switch (printOperation->m_pageSet) {
    case GTK_PAGE_SET_ODD:
        if (reverse) {
            lastPagePosition = std::min(numberUp - 1, pages.size() - 1);
            sheetNumber = (numberOfSheets - 1) - (numberOfSheets - 1) % 2;
        } else
            lastPagePosition = std::min(((numberOfSheets - 1) - ((numberOfSheets - 1) % 2)) * numberUp - 1, pages.size() - 1);
        break;
    case GTK_PAGE_SET_EVEN:
        if (reverse) {
            lastPagePosition = std::min(2 * numberUp - 1, pages.size() - 1);
            sheetNumber = (numberOfSheets - 1) - (1 - (numberOfSheets - 1) % 2);
        } else {
            lastPagePosition = std::min(((numberOfSheets - 1) - (1 - (numberOfSheets - 1) % 2)) * numberUp - 1, pages.size() - 1);
            sheetNumber = numberOfSheets > 1 ? 1 : -1;
        }
        break;
    case GTK_PAGE_SET_ALL:
        if (reverse) {
            lastPagePosition = std::min(numberUp - 1, pages.size() - 1);
            sheetNumber = pages.size() - 1;
        } else
            lastPagePosition = pages.size() - 1;
        break;
    }

    if (sheetNumber * numberUp >= pages.size()) {
        isValid = false;
        return;
    }

    printOperation->m_pagePosition = sheetNumber * numberUp;
    pageNumber = pages[printOperation->m_pagePosition];
    firstPagePosition = printOperation->m_pagePosition;
    firstSheetNumber = sheetNumber;
}

void WebPrintOperationGtk::PrintPagesData::incrementPageSequence()
{
    if (totalPrinted == -1) {
        totalPrinted = 0;
        return;
    }

    size_t pagePosition = printOperation->m_pagePosition;
    if (pagePosition == lastPagePosition && !copiesLeft()) {
        isDone = true;
        return;
    }

    if (pagePosition == lastPagePosition && uncollatedCopiesLeft()) {
        pagePosition = firstPagePosition;
        sheetNumber = firstSheetNumber;
        uncollated++;
    } else if (printOperation->currentPageIsLastPageOfSheet()) {
        if (!collatedCopiesLeft()) {
            int step = printOperation->m_pageSet == GTK_PAGE_SET_ALL ? 1 : 2;
            step *= printOperation->m_reverse ? -1 : 1;
            sheetNumber += step;
            collated = 0;
        } else
            collated++;
        pagePosition = sheetNumber * printOperation->m_numberUp;
    } else
        pagePosition++;
    printOperation->m_pagePosition = pagePosition;

    if (pagePosition >= pages.size() || sheetNumber >= numberOfSheets) {
        isDone = true;
        return;
    }
    pageNumber = pages[pagePosition];
    totalPrinted++;
}

WebPrintOperationGtk::WebPrintOperationGtk(const PrintInfo& printInfo)
    : m_printSettings(printInfo.printSettings.get())
    , m_pageSetup(printInfo.pageSetup.get())
    , m_printMode(printInfo.printMode)
{
}

WebPrintOperationGtk::~WebPrintOperationGtk()
{
    if (m_printPagesIdleId)
        g_source_remove(m_printPagesIdleId);
}

void WebPrintOperationGtk::startPrint(WebCore::PrintContext* printContext, CompletionHandler<void(RefPtr<WebCore::FragmentedSharedBuffer>&&, WebCore::ResourceError&&)>&& completionHandler)
{
    m_printContext = printContext;
    m_completionHandler = WTFMove(completionHandler);

    const char* outputFormat = gtk_print_settings_get(m_printSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);

#if USE(CAIRO)
    m_buffer.reset();

    auto writeCairoStream = [](void* userData, const unsigned char* data, unsigned length) -> cairo_status_t {
        auto& printOperation = *static_cast<WebPrintOperationGtk*>(userData);
        printOperation.m_buffer.append(std::span { data, length });
        return CAIRO_STATUS_SUCCESS;
    };

    auto* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
    double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_POINTS);
    double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_POINTS);
    RefPtr<cairo_surface_t> surface;
    if (!g_strcmp0(outputFormat, "pdf"))
        surface = adoptRef(cairo_pdf_surface_create_for_stream(writeCairoStream, this, width, height));
    else if (!g_strcmp0(outputFormat, "ps"))
        surface = adoptRef(cairo_ps_surface_create_for_stream(writeCairoStream, this, width, height));
    else if (!g_strcmp0(outputFormat, "svg")) {
        surface = adoptRef(cairo_svg_surface_create_for_stream(writeCairoStream, this, width, height));

        const cairo_svg_version_t* versions;
        int versionsCount;
        cairo_svg_get_versions(&versions, &versionsCount);
        if (versionsCount)
            cairo_svg_surface_restrict_to_version(surface.get(), versions[versionsCount - 1]);
    }

    auto lpi = gtk_print_settings_get_printer_lpi(m_printSettings.get());
    cairo_surface_set_fallback_resolution(surface.get(), 2.0 * lpi, 2.0 * lpi);
#elif USE(SKIA)
    RELEASE_ASSERT(!g_strcmp0(outputFormat, "pdf"));
#endif

    int rangesCount;
    m_pageRanges = gtk_print_settings_get_page_ranges(m_printSettings.get(), &rangesCount);
    m_pageRangesCount = rangesCount;
    m_pagesToPrint = gtk_print_settings_get_print_pages(m_printSettings.get());
    m_needsRotation = gtk_print_settings_get_bool(m_printSettings.get(), "wk-rotate-to-orientation");

    // Manual capabilities.
    m_numberUp = gtk_print_settings_get_number_up(m_printSettings.get());
    m_numberUpLayout = gtk_print_settings_get_number_up_layout(m_printSettings.get());
    m_pageSet = gtk_print_settings_get_page_set(m_printSettings.get());
    m_reverse = gtk_print_settings_get_reverse(m_printSettings.get());
    m_copies = gtk_print_settings_get_n_copies(m_printSettings.get());
    m_collateCopies = gtk_print_settings_get_collate(m_printSettings.get());
    m_scale = gtk_print_settings_get_scale(m_printSettings.get());

#if USE(CAIRO)
    print(surface.get(), 72, 72);
#elif USE(SKIA)
    print(72, 72);
#endif
}

#if USE(CAIRO)
void WebPrintOperationGtk::startPage(cairo_t* cr)
{
    if (!currentPageIsFirstPageOfSheet())
        return;

    GtkPaperSize* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
    double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_POINTS);
    double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_POINTS);

    cairo_surface_t* surface = cairo_get_target(cr);
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
    } else if (surfaceType == CAIRO_SURFACE_TYPE_PDF) {
        switch (gtk_page_setup_get_orientation(m_pageSetup.get())) {
        case GTK_PAGE_ORIENTATION_PORTRAIT:
        case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
            cairo_pdf_surface_set_size(surface, width, height);
            break;
        case GTK_PAGE_ORIENTATION_LANDSCAPE:
        case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
            cairo_pdf_surface_set_size(surface, height, width);
            break;
        }
    }
}

void WebPrintOperationGtk::endPage(cairo_t* cr)
{
    if (currentPageIsLastPageOfSheet())
        cairo_show_page(cr);
}

void WebPrintOperationGtk::endPrint(cairo_t* cr)
{
    cairo_surface_finish(cairo_get_target(cr));
    printDone(m_buffer.take(), { });
}
#elif USE(SKIA)
void WebPrintOperationGtk::startPage(SkPictureRecorder& recorder)
{
    if (!currentPageIsFirstPageOfSheet()) {
        ASSERT(m_pageCanvas);
        return;
    }

    ASSERT(!m_pageCanvas);

    GtkPaperSize* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
    double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_POINTS);
    double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_POINTS);

    switch (gtk_page_setup_get_orientation(m_pageSetup.get())) {
    case GTK_PAGE_ORIENTATION_PORTRAIT:
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
        m_pageCanvas = recorder.beginRecording(width, height);
        break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
        m_pageCanvas = recorder.beginRecording(height, width);
        break;
    }

    ASSERT(m_pageCanvas);
}

void WebPrintOperationGtk::endPage(SkPictureRecorder& recorder)
{
    ASSERT(m_pageCanvas);

    if (currentPageIsLastPageOfSheet()) {
        m_pages.append(recorder.finishRecordingAsPicture());
        m_pageCanvas = nullptr;
    }
}

void WebPrintOperationGtk::endPrint()
{
    SkDynamicMemoryWStream memoryBuffer;

    auto document = SkPDF::MakeDocument(&memoryBuffer);
    ASSERT(document);
    for (auto page : m_pages) {
        const auto& rect = page->cullRect();
        auto canvas = document->beginPage(rect.width(), rect.height());
        canvas->drawPicture(page);
        document->endPage();
    }
    document->close();

    printDone(WebCore::FragmentedSharedBuffer::create(memoryBuffer.detachAsData()), { });

    m_pages.clear();
}
#endif

int WebPrintOperationGtk::pageCount() const
{
    return m_printContext ? m_printContext->pageCount() : 0;
}

bool WebPrintOperationGtk::currentPageIsFirstPageOfSheet() const
{
    return (m_numberUp < 2 || !((m_pagePosition) % m_numberUp));
}

bool WebPrintOperationGtk::currentPageIsLastPageOfSheet() const
{
    return (m_numberUp < 2 || !((m_pagePosition + 1) % m_numberUp) || m_pagePosition == m_numberOfPagesToPrint - 1);
}

URL WebPrintOperationGtk::frameURL() const
{
    if (!m_printContext)
        return URL();

    WebCore::DocumentLoader* documentLoader = m_printContext->frame()->loader().documentLoader();
    return documentLoader ? documentLoader->url() : URL();
}

void WebPrintOperationGtk::rotatePageIfNeeded()
{
    if (!m_needsRotation)
        return;

    GtkPaperSize* paperSize = gtk_page_setup_get_paper_size(m_pageSetup.get());
    double width = gtk_paper_size_get_width(paperSize, GTK_UNIT_INCH) * m_xDPI;
    double height = gtk_paper_size_get_height(paperSize, GTK_UNIT_INCH) * m_yDPI;

#if USE(CAIRO)
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
#elif USE(SKIA)
    switch (gtk_page_setup_get_orientation(m_pageSetup.get())) {
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
        m_pageCanvas->translate(0, height);
        m_pageCanvas->rotate(90);
        break;
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
        m_pageCanvas->translate(width, height);
        m_pageCanvas->scale(-1, -1);
        break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
        m_pageCanvas->translate(width, 0);
        m_pageCanvas->rotate(90);
        m_pageCanvas->scale(-1, 1);
        break;
    case GTK_PAGE_ORIENTATION_PORTRAIT:
    default:
        break;
    }
#endif
}

void WebPrintOperationGtk::getRowsAndColumnsOfPagesPerSheet(size_t& rows, size_t& columns)
{
    switch (m_numberUp) {
    default:
        columns = 1;
        rows = 1;
        break;
    case 2:
        columns = 2;
        rows = 1;
        break;
    case 4:
        columns = 2;
        rows = 2;
        break;
    case 6:
        columns = 3;
        rows = 2;
        break;
    case 9:
        columns = 3;
        rows = 3;
        break;
    case 16:
        columns = 4;
        rows = 4;
        break;
    }
}

void WebPrintOperationGtk::getPositionOfPageInSheet(size_t rows, size_t columns, int& x, int&y)
{
    switch (m_numberUpLayout) {
    case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM:
        x = m_pagePosition % columns;
        y = (m_pagePosition / columns) % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_BOTTOM_TO_TOP:
        x = m_pagePosition % columns;
        y = rows - 1 - (m_pagePosition / columns) % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM:
        x = columns - 1 - m_pagePosition % columns;
        y = (m_pagePosition / columns) % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_BOTTOM_TO_TOP:
        x = columns - 1 - m_pagePosition % columns;
        y = rows - 1 - (m_pagePosition / columns) % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_LEFT_TO_RIGHT:
        x = (m_pagePosition / rows) % columns;
        y = m_pagePosition % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_TOP_TO_BOTTOM_RIGHT_TO_LEFT:
        x = columns - 1 - (m_pagePosition / rows) % columns;
        y = m_pagePosition % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_LEFT_TO_RIGHT:
        x = (m_pagePosition / rows) % columns;
        y = rows - 1 - m_pagePosition % rows;
        break;
    case GTK_NUMBER_UP_LAYOUT_BOTTOM_TO_TOP_RIGHT_TO_LEFT:
        x = columns - 1 - (m_pagePosition / rows) % columns;
        y = rows - 1 - m_pagePosition % rows;
        break;
    }
}

void WebPrintOperationGtk::prepareContextToDraw()
{
    if (m_numberUp < 2) {
        double left = gtk_page_setup_get_left_margin(m_pageSetup.get(), GTK_UNIT_INCH);
        double top = gtk_page_setup_get_top_margin(m_pageSetup.get(), GTK_UNIT_INCH);
#if USE(CAIRO)
        if (m_scale != 1.0)
            cairo_scale(m_cairoContext.get(), m_scale, m_scale);
        rotatePageIfNeeded();
        cairo_translate(m_cairoContext.get(), left * m_xDPI, top * m_yDPI);
#elif USE(SKIA)
        if (m_scale != 1.0)
            m_pageCanvas->scale(m_scale, m_scale);
        rotatePageIfNeeded();
        m_pageCanvas->translate(left * m_xDPI, top * m_yDPI);
#endif
        return;
    }

    rotatePageIfNeeded();

    // Multiple pages per sheet.
    double marginLeft = gtk_page_setup_get_left_margin(m_pageSetup.get(), GTK_UNIT_POINTS);
    double marginRight = gtk_page_setup_get_right_margin(m_pageSetup.get(), GTK_UNIT_POINTS);
    double marginTop = gtk_page_setup_get_top_margin(m_pageSetup.get(), GTK_UNIT_POINTS);
    double marginBottom = gtk_page_setup_get_bottom_margin(m_pageSetup.get(), GTK_UNIT_POINTS);

    double paperWidth = gtk_page_setup_get_paper_width(m_pageSetup.get(), GTK_UNIT_POINTS);
    double paperHeight = gtk_page_setup_get_paper_height(m_pageSetup.get(), GTK_UNIT_POINTS);

    size_t rows, columns;
    getRowsAndColumnsOfPagesPerSheet(rows, columns);

    GtkPageOrientation orientation = gtk_page_setup_get_orientation(m_pageSetup.get());
    double pageWidth = 0, pageHeight = 0;
    switch (orientation) {
    case GTK_PAGE_ORIENTATION_PORTRAIT:
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
        pageWidth = paperWidth - (marginLeft + marginRight);
        pageHeight = paperHeight - (marginTop + marginBottom);
#if USE(CAIRO)
        cairo_translate(m_cairoContext.get(), marginLeft, marginTop);
#elif USE(SKIA)
        m_pageCanvas->translate(marginLeft, marginTop);
#endif
        break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
        pageWidth = paperWidth - (marginTop + marginBottom);
        pageHeight = paperHeight - (marginLeft + marginRight);
#if USE(CAIRO)
        cairo_translate(m_cairoContext.get(), marginTop, marginLeft);
#elif USE(SKIA)
        m_pageCanvas->translate(marginTop, marginLeft);
#endif

        size_t tmp = columns;
        columns = rows;
        rows = tmp;
        break;
    }

    int x, y;
    getPositionOfPageInSheet(rows, columns, x, y);

    switch (m_numberUp) {
    case 4:
    case 9:
    case 16: {
        double scaleX = pageWidth / (columns * paperWidth);
        double scaleY = pageHeight / (rows * paperHeight);
        double scale = std::min(scaleX, scaleY);

        double stepX = paperWidth * (scaleX / scale);
        double stepY = paperHeight * (scaleY / scale);

        double width = gtk_page_setup_get_page_width(m_pageSetup.get(), GTK_UNIT_INCH) * m_xDPI;
        double height = gtk_page_setup_get_page_height(m_pageSetup.get(), GTK_UNIT_INCH) * m_yDPI;

        double offsetX, offsetY;
        if (marginLeft + marginRight > 0) {
            offsetX = marginLeft * (stepX - width) / (marginLeft + marginRight);
            offsetY = marginTop * (stepY - height) / (marginTop + marginBottom);
        } else {
            offsetX = (stepX - width) / 2.0;
            offsetY = (stepY - height) / 2.0;
        }

#if USE(CAIRO)
        cairo_scale(m_cairoContext.get(), scale, scale);
        cairo_translate(m_cairoContext.get(), x * stepX + offsetX, y * stepY + offsetY);
        if (m_scale != 1.0)
            cairo_scale(m_cairoContext.get(), m_scale, m_scale);
#elif USE(SKIA)
        m_pageCanvas->scale(scale, scale);
        m_pageCanvas->translate(x * stepX + offsetX, y * stepY + offsetY);
        if (m_scale != 1.0)
            m_pageCanvas->scale(m_scale, m_scale);
#endif
        break;
    }
    case 2:
    case 6: {
        double scaleX = pageHeight / (columns * paperWidth);
        double scaleY = pageWidth / (rows * paperHeight);
        double scale = std::min(scaleX, scaleY);

        double stepX = paperWidth * (scaleX / scale);
        double stepY = paperHeight * (scaleY / scale);

        double offsetX = ((stepX - paperWidth) / 2.0 * columns) - marginRight;
        double offsetY = ((stepY - paperHeight) / 2.0 * rows) + marginTop;

#if USE(CAIRO)
        cairo_scale(m_cairoContext.get(), scale, scale);
        cairo_translate(m_cairoContext.get(), y * paperHeight + offsetY, (columns - x) * paperWidth + offsetX);
        if (m_scale != 1.0)
            cairo_scale(m_cairoContext.get(), m_scale, m_scale);
        cairo_rotate(m_cairoContext.get(), -G_PI / 2);
#elif USE(SKIA)
        m_pageCanvas->scale(scale, scale);
        m_pageCanvas->translate(y * paperHeight + offsetY, (columns - x) * paperWidth + offsetX);
        if (m_scale != 1.0)
            m_pageCanvas->scale(m_scale, m_scale);
        m_pageCanvas->rotate(-90);
#endif
        break;
    }
    default:
        break;
    }
}

void WebPrintOperationGtk::renderPage(int pageNumber)
{
#if USE(CAIRO)
    startPage(m_cairoContext.get());
    cairo_save(m_cairoContext.get());
#elif USE(SKIA)
    SkPictureRecorder recorder;
    startPage(recorder);
    m_pageCanvas->save();
#endif

    prepareContextToDraw();

    double pageWidth = gtk_page_setup_get_page_width(m_pageSetup.get(), GTK_UNIT_INCH) * m_xDPI;
#if USE(CAIRO)
    WebCore::GraphicsContextCairo graphicsContext(m_cairoContext.get());
#elif USE(SKIA)
    WebCore::GraphicsContextSkia graphicsContext(*m_pageCanvas, WebCore::RenderingMode::Unaccelerated, WebCore::RenderingPurpose::Unspecified);
#endif
    m_printContext->spoolPage(graphicsContext, pageNumber, pageWidth / m_scale);

#if USE(CAIRO)
    cairo_restore(m_cairoContext.get());
    endPage(m_cairoContext.get());
#elif USE(SKIA)
    m_pageCanvas->restore();
    endPage(recorder);
#endif
}

gboolean WebPrintOperationGtk::printPagesIdle(gpointer userData)
{
    auto* data = static_cast<PrintPagesData*>(userData);
    data->incrementPageSequence();
    if (data->isDone)
        return FALSE;

    data->printOperation->renderPage(data->pageNumber);
    return TRUE;
}

void WebPrintOperationGtk::printPagesIdleDone(gpointer userData)
{
    std::unique_ptr<PrintPagesData> data(static_cast<PrintPagesData*>(userData));
    if (data->mainLoop)
        g_main_loop_quit(data->mainLoop.get());

    data->printOperation->printPagesDone();
}

void WebPrintOperationGtk::printPagesDone()
{
    m_printPagesIdleId = 0;
#if USE(CAIRO)
    endPrint(m_cairoContext.get());
    m_cairoContext = nullptr;
#elif USE(SKIA)
    endPrint();
#endif
}

void WebPrintOperationGtk::printDone(RefPtr<WebCore::FragmentedSharedBuffer>&& buffer, WebCore::ResourceError&& error)
{
    if (m_printPagesIdleId)
        g_source_remove(m_printPagesIdleId);
    m_printPagesIdleId = 0;

    // Print finished or failed, notify the UI process that we are done if the page hasn't been closed.
    if (m_completionHandler)
        m_completionHandler(WTFMove(buffer), WTFMove(error));
}

#if USE(CAIRO)
void WebPrintOperationGtk::print(cairo_surface_t* surface, double xDPI, double yDPI)
#elif USE(SKIA)
void WebPrintOperationGtk::print(double xDPI, double yDPI)
#endif
{
    ASSERT(m_printContext);

    auto data = makeUnique<PrintPagesData>(this);
    if (!data->isValid) {
#if USE(CAIRO)
        cairo_surface_finish(surface);
#endif
        printDone(nullptr, invalidPageRangeToPrint(frameURL()));
        return;
    }

    m_xDPI = xDPI;
    m_yDPI = yDPI;
#if USE(CAIRO)
    m_cairoContext = adoptRef(cairo_create(surface));
#endif

    // Make sure the print pages idle has more priority than IPC messages comming from
    // the IO thread, so that the EndPrinting message is always handled once the print
    // operation has finished. See https://bugs.webkit.org/show_bug.cgi?id=122801.
    unsigned idlePriority = m_printMode == PrintInfo::PrintMode::Sync ? G_PRIORITY_DEFAULT - 10 : G_PRIORITY_DEFAULT_IDLE + 10;
    GMainLoop* mainLoop = data->mainLoop.get();
    m_printPagesIdleId = g_idle_add_full(idlePriority, printPagesIdle, data.release(), printPagesIdleDone);
    if (m_printMode == PrintInfo::PrintMode::Sync) {
        ASSERT(mainLoop);
        g_main_loop_run(mainLoop);
    }
}

} // namespace WebKit

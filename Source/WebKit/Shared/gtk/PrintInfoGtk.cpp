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
#include "PrintInfo.h"

#include <gtk/gtk.h>
#if HAVE(GTK_UNIX_PRINTING)
#include <gtk/gtkunixprint.h>
#endif

namespace WebKit {

#if HAVE(GTK_UNIX_PRINTING)
PrintInfo::PrintInfo(GtkPrintJob* job, PrintMode printMode)
    : printMode(printMode)
{
    ASSERT(job);

    GRefPtr<GtkPrintSettings> jobSettings;
    GRefPtr<GtkPageSetup> jobPageSetup;
    g_object_get(job, "settings", &jobSettings.outPtr(), "page-setup", &jobPageSetup.outPtr(), nullptr);

    pageSetupScaleFactor = gtk_print_settings_get_scale(jobSettings.get()) / 100.0;
    availablePaperWidth = gtk_page_setup_get_paper_width(jobPageSetup.get(), GTK_UNIT_POINTS) - gtk_page_setup_get_left_margin(jobPageSetup.get(), GTK_UNIT_POINTS) - gtk_page_setup_get_right_margin(jobPageSetup.get(), GTK_UNIT_POINTS);
    availablePaperHeight = gtk_page_setup_get_paper_height(jobPageSetup.get(), GTK_UNIT_POINTS) - gtk_page_setup_get_top_margin(jobPageSetup.get(), GTK_UNIT_POINTS) - gtk_page_setup_get_bottom_margin(jobPageSetup.get(), GTK_UNIT_POINTS);
    margin = { gtk_page_setup_get_top_margin(jobPageSetup.get(), GTK_UNIT_POINTS), gtk_page_setup_get_right_margin(jobPageSetup.get(), GTK_UNIT_POINTS), gtk_page_setup_get_bottom_margin(jobPageSetup.get(), GTK_UNIT_POINTS), gtk_page_setup_get_left_margin(jobPageSetup.get(), GTK_UNIT_POINTS) };

    pageSetup = WTFMove(jobPageSetup);

    printSettings = adoptGRef(gtk_print_settings_new());
    gtk_print_settings_set_printer_lpi(printSettings.get(), gtk_print_settings_get_printer_lpi(jobSettings.get()));

    if (const char* outputFormat = gtk_print_settings_get(jobSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT))
        gtk_print_settings_set(printSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, outputFormat);
    else {
        auto* printer = gtk_print_job_get_printer(job);
        if (gtk_printer_accepts_pdf(printer))
            gtk_print_settings_set(printSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "pdf");
        else if (gtk_printer_accepts_ps(printer))
            gtk_print_settings_set(printSettings.get(), GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "ps");
    }

    int rangesCount;
    auto* pageRanges = gtk_print_job_get_page_ranges(job, &rangesCount);
    gtk_print_settings_set_page_ranges(printSettings.get(), pageRanges, rangesCount);
    gtk_print_settings_set_print_pages(printSettings.get(), gtk_print_job_get_pages(job));
    gtk_print_settings_set_bool(printSettings.get(), "wk-rotate-to-orientation", gtk_print_job_get_rotate(job));
    gtk_print_settings_set_number_up(printSettings.get(), gtk_print_job_get_n_up(job));
    gtk_print_settings_set_number_up_layout(printSettings.get(), gtk_print_job_get_n_up_layout(job));
    gtk_print_settings_set_page_set(printSettings.get(), gtk_print_job_get_page_set(job));
    gtk_print_settings_set_reverse(printSettings.get(), gtk_print_job_get_reverse(job));
    gtk_print_settings_set_n_copies(printSettings.get(), gtk_print_job_get_num_copies(job));
    gtk_print_settings_set_collate(printSettings.get(), gtk_print_job_get_collate(job));
    gtk_print_settings_set_scale(printSettings.get(), gtk_print_job_get_scale(job));
}
#endif

} // namespace WebKit

/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "PrinterListGtk.h"

#ifdef HAVE_GTK_UNIX_PRINTING

#include <gtk/gtkunixprint.h>

namespace WebKit {

PrinterListGtk* PrinterListGtk::s_sharedPrinterList = nullptr;

RefPtr<PrinterListGtk> PrinterListGtk::shared()
{
    if (s_sharedPrinterList)
        return s_sharedPrinterList;

    return adoptRef(new PrinterListGtk);
}

gboolean PrinterListGtk::enumeratePrintersFunction(GtkPrinter* printer)
{
    ASSERT(s_sharedPrinterList);
    s_sharedPrinterList->addPrinter(printer);
    return FALSE;
}

PrinterListGtk::PrinterListGtk()
    : m_defaultPrinter(nullptr)
{
    ASSERT(!s_sharedPrinterList);
    s_sharedPrinterList = this;
    gtk_enumerate_printers(reinterpret_cast<GtkPrinterFunc>(&enumeratePrintersFunction), nullptr, nullptr, TRUE);
}

PrinterListGtk::~PrinterListGtk()
{
    ASSERT(s_sharedPrinterList);
    s_sharedPrinterList = nullptr;
}

void PrinterListGtk::addPrinter(GtkPrinter* printer)
{
    m_printerList.append(printer);
    if (gtk_printer_is_default(printer))
        m_defaultPrinter = printer;
}

GtkPrinter* PrinterListGtk::findPrinter(const char* printerName) const
{
    for (auto& printer : m_printerList) {
        if (!strcmp(printerName, gtk_printer_get_name(printer.get())))
            return printer.get();
    }
    return nullptr;
}

} // namespace WebKit

#endif // HAVE_GTK_UNIX_PRINTING

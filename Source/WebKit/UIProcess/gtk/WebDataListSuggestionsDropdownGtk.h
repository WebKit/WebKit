/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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

#pragma once

#if ENABLE(DATALIST_ELEMENT)

#include "WebDataListSuggestionsDropdown.h"

typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;

namespace WebKit {

class WebPageProxy;

class WebDataListSuggestionsDropdownGtk final : public WebDataListSuggestionsDropdown {
public:
    static Ref<WebDataListSuggestionsDropdownGtk> create(GtkWidget* webView, WebPageProxy& page)
    {
        return adoptRef(*new WebDataListSuggestionsDropdownGtk(webView, page));
    }

    ~WebDataListSuggestionsDropdownGtk();

private:
    WebDataListSuggestionsDropdownGtk(GtkWidget*, WebPageProxy&);

    void show(WebCore::DataListSuggestionInformation&&) final;
    void handleKeydownWithIdentifier(const String&) final;
    void close() final;

    static void treeViewRowActivatedCallback(GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, WebDataListSuggestionsDropdownGtk*);
    void didSelectOption(const String&);

    GtkWidget* m_webView { nullptr };
    GtkWidget* m_popup { nullptr };
    GtkWidget* m_treeView { nullptr };
};

} // namespace WebKit

#endif // ENABLE(DATALIST_ELEMENT)

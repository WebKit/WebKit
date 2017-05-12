/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebContextMenuItem_h
#define WebContextMenuItem_h

#if ENABLE(CONTEXT_MENUS)

#include "APIObject.h"
#include "WebContextMenuItemData.h"

namespace API {
class Array;
}

namespace WebCore {
class ContextMenuItem;
}

namespace WebKit {

class WebContextMenuItem : public API::ObjectImpl<API::Object::Type::ContextMenuItem> {
public:
    static Ref<WebContextMenuItem> create(const WebContextMenuItemData& data)
    {
        return adoptRef(*new WebContextMenuItem(data));
    }

    static Ref<WebContextMenuItem> create(const String& title, bool enabled, API::Array* submenuItems);
    static WebContextMenuItem* separatorItem();

    Ref<API::Array> submenuItemsAsAPIArray() const;

    API::Object* userData() const;
    void setUserData(API::Object*);

    const WebContextMenuItemData& data() { return m_webContextMenuItemData; }

private:
    WebContextMenuItem(const WebContextMenuItemData&);

    WebContextMenuItemData m_webContextMenuItemData;
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
#endif // WebContextMenuItem_h

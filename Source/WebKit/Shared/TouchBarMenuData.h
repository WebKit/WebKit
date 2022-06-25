/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(TOUCH_BAR)

#include "TouchBarMenuItemData.h"
#include <wtf/Vector.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HTMLMenuElement;
}

namespace WebKit {

class TouchBarMenuData {
public:
    TouchBarMenuData() = default;
    explicit TouchBarMenuData(const WebCore::HTMLMenuElement&);

    void addMenuItem(const TouchBarMenuItemData&);
    void removeMenuItem(const TouchBarMenuItemData&);
    
    const Vector<TouchBarMenuItemData>& items() const { return m_items; }
    
    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, TouchBarMenuData&);
    
    void setID(const String& identifier) { m_id = identifier; }
    bool isPageCustomized() const { return m_isPageCustomized; }
    void setIsPageCustomized(bool customized) { m_isPageCustomized = customized; }
    
private:
    Vector<TouchBarMenuItemData> m_items;
    String m_id;
    
    bool m_isPageCustomized { false };
};

}

#endif // HAVE(TOUCH_BAR)

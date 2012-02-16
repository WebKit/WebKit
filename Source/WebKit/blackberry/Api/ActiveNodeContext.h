/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef ActiveNodeContext_h
#define ActiveNodeContext_h

#include "WebString.h"

namespace BlackBerry {
namespace WebKit {

class ActiveNodeContext {
public:
    // We assume a default context should be selectable, but nothing else.
    ActiveNodeContext()
        : m_flags(IsSelectable)
        {
        }

    enum ContextFlags { IsFocusable = 0x01,
                        IsInput = 0x02,
                        IsPassword = 0x04,
                        IsSelectable = 0x08,
                        IsSingleLine = 0x10, };

    bool isFocusable() const { return m_flags & IsFocusable; }
    bool isInput() const { return m_flags & IsInput; }
    bool isPassword() const { return m_flags & IsPassword; }
    bool isSelectable() const { return m_flags & IsSelectable; }
    bool isSingleLine() const { return m_flags & IsSingleLine; }

    void setFlag(ContextFlags flag) { m_flags |= flag; }
    void resetFlag(ContextFlags flag) { m_flags &= ~flag; }

    const WebString& imageAlt() const { return m_imageAlt; }
    void setImageAlt(const WebString& string) { m_imageAlt = string; }

    const WebString& imageSrc() const { return m_imageSrc; }
    void setImageSrc(const WebString& string) { m_imageSrc = string; }

    const WebString& pattern() const { return m_pattern; }
    void setPattern(const WebString& string) { m_pattern = string; }

    const WebString& text() const { return m_text; }
    void setText(const WebString& string) { m_text = string; }

    const WebString& url() const { return m_url; }
    void setUrl(const WebString& string) { m_url = string; }

private:
    unsigned m_flags;
    WebString m_imageAlt;
    WebString m_imageSrc;
    WebString m_pattern;
    WebString m_text;
    WebString m_url;
};

}
}
#endif // ActiveNodeContext_h

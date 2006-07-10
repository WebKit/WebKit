/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PopUpButton_h
#define PopUpButton_h

#include "ListBox.h"

#ifdef __OBJC__
@class WebCorePopUpButtonAdapter;
#else
class WebCorePopUpButtonAdapter;
class NSMenuItem;
class NSFont;
#endif

namespace WebCore {

class PopUpButton : public Widget {
public:  
    PopUpButton();
    ~PopUpButton();
    
    void clear();
    void appendItem(const DeprecatedString& text, bool enabled) { appendItem(text, ListBoxOption, enabled); }
    void appendGroupLabel(const DeprecatedString& text) { appendItem(text, ListBoxGroupLabel, false); }
    void appendSeparator() { appendItem(DeprecatedString::null, ListBoxSeparator, true); }

    int currentItem() const { return _currentItem; }
    void setCurrentItem(int);
    
    IntSize sizeHint() const;
    IntRect frameGeometry() const;
    void setFrameGeometry(const IntRect&);
    int baselinePosition(int height) const;
    void setFont(const Font&);

    void itemSelected();
    
    virtual FocusPolicy focusPolicy() const;

    void setWritingDirection(TextDirection);

    virtual void populate();
    void populateMenu();
    
private:
    void appendItem(const DeprecatedString&, ListBoxItemType, bool);
    const int* dimensions() const;
    NSFont* labelFont() const;
    void setTitle(NSMenuItem*, const ListBoxItem&);
    
    mutable int _width;
    mutable bool _widthGood;

    mutable int _currentItem;

    // A Vector<ListBoxItem> may be more efficient for large menus.
    DeprecatedValueList<ListBoxItem> _items;
    mutable bool _menuPopulated;

    mutable NSFont *_labelFont;
};

}

#endif

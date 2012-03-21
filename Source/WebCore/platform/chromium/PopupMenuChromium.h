/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PopupMenuChromium_h
#define PopupMenuChromium_h

#include "config.h"

#include "PopupMenu.h"
#include "PopupMenuPrivate.h"

namespace WebCore {

class ChromeClientChromium;
class FrameView;
class IntRect;
struct PopupItem;
class PopupMenuClient;

class PopupMenuChromium : public PopupMenu {
public:
    PopupMenuChromium(PopupMenuClient*);
    ~PopupMenuChromium();

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

    static int minimumRowHeight() { return s_minimumRowHeight; }
    static void setMinimumRowHeight(int minimumRowHeight) { s_minimumRowHeight = minimumRowHeight; }

    static int optionPaddingForTouch() { return s_optionPaddingForTouch; }
    static void setOptionPaddingForTouch(int optionPaddingForTouch) { s_optionPaddingForTouch = optionPaddingForTouch; }

private:
    PopupMenuClient* client() const { return m_popupClient; }

    PopupMenuClient* m_popupClient;
    PopupMenuPrivate p;

    static int s_minimumRowHeight;
    static int s_optionPaddingForTouch;
};

} // namespace WebCore

#endif

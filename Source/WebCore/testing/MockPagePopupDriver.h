/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockPagePopupDriver_h
#define MockPagePopupDriver_h

#include "PagePopupClient.h"
#include "PagePopupDriver.h"
#include <wtf/RefPtr.h>

#if ENABLE(PAGE_POPUP)
namespace WebCore {

class Frame;
class IntRect;
class MockPagePopup;
class PagePopup;
class PagePopupController;

class MockPagePopupDriver : public PagePopupDriver {
public:
    static PassOwnPtr<MockPagePopupDriver> create(Frame* mainFrame);
    virtual ~MockPagePopupDriver();
    PagePopupController* pagePopupController() { return m_pagePopupController.get(); }

private:
    MockPagePopupDriver(Frame* mainFrame);

    // PagePopupDriver functions:
    virtual PagePopup* openPagePopup(PagePopupClient*, const IntRect& originBoundsInRootView) OVERRIDE;
    virtual void closePagePopup(PagePopup*) OVERRIDE;

    RefPtr<MockPagePopup> m_mockPagePopup;
    Frame* m_mainFrame;
    RefPtr<PagePopupController> m_pagePopupController;
};

}
#endif
#endif

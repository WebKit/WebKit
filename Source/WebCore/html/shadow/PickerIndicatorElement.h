/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef PickerIndicatorElement_h
#define PickerIndicatorElement_h

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeChooser.h"
#include "DateTimeChooserClient.h"
#include "HTMLDivElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLInputElement;

class PickerIndicatorElement FINAL : public HTMLDivElement, public DateTimeChooserClient {
public:
    // PickerIndicatorOwner implementer must call removePickerIndicatorOwner when
    // it doesn't handle event, e.g. at destruction.
    class PickerIndicatorOwner {
    public:
        virtual ~PickerIndicatorOwner() { }
        virtual bool isPickerIndicatorOwnerDisabledOrReadOnly() const = 0;
        virtual void pickerIndicatorChooseValue(const String&) = 0;
        virtual bool setupDateTimeChooserParameters(DateTimeChooserParameters&) = 0;
    };

    static PassRefPtr<PickerIndicatorElement> create(Document*, PickerIndicatorOwner&);
    virtual ~PickerIndicatorElement();
    void openPopup();
    void closePopup();
    virtual bool willRespondToMouseClickEvents() OVERRIDE;
    void removePickerIndicatorOwner() { m_pickerIndicatorOwner = 0; }

    // DateTimeChooserClient implementation.
    virtual void didChooseValue(const String&) OVERRIDE;
    virtual void didEndChooser() OVERRIDE;

private:
    PickerIndicatorElement(Document*, PickerIndicatorOwner&);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual void detach() OVERRIDE;

    HTMLInputElement* hostInput();

    PickerIndicatorOwner* m_pickerIndicatorOwner;
    RefPtr<DateTimeChooser> m_chooser;
};

}
#endif
#endif

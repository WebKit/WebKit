/*
 * Copyright (C) 2014 Haiku, Inc.  All rights reserved.
 *
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
#include "config.h"

#include <WebCore/DateTimeChooser.h>
#include <WebCore/DateTimeChooserClient.h>
#include <WebCore/platform/DateTimeChooserParameters.h>
#include <WebCore/InputTypeNames.h>

#include <support/Locker.h>
#include <locale/Collator.h>
#include <private/shared/CalendarView.h>
#include <LocaleRoster.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <SeparatorView.h>
#include <TextControl.h>
#include <TimeFormat.h>

namespace WebCore {

class DateTimeChooserWindow: public BWindow
{
public:
    DateTimeChooserWindow(DateTimeChooserClient* client)
        : BWindow(BRect(0, 0, 10, 10), "Date Picker", B_FLOATING_WINDOW,
            B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
        , m_calendar(nullptr)
        , m_client(client)
    {
        BGroupLayout* root = new BGroupLayout(B_VERTICAL);
        root->SetSpacing(0);
        SetLayout(root);
        fMainGroup = new BGroupView(B_HORIZONTAL);
        fMainGroup->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        fMainGroup->GroupLayout()->SetInsets(5, 5, 5, 5);
        AddChild(fMainGroup);

        m_okButton = new BButton("ok", "Done", new BMessage('done'));
        BButton* cancel = new BButton("cancel", "Cancel", new BMessage('canc'));

        BGroupView* bottomGroup = new BGroupView(B_HORIZONTAL);
        bottomGroup->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        BGroupLayoutBuilder(bottomGroup)
            .SetInsets(5, 5, 5, 5)
            .AddGlue()
            .Add(cancel)
            .Add(m_okButton);
        AddChild(bottomGroup);

    }

    void Configure(const WebCore::DateTimeChooserParameters& params) {
        MoveTo(BRect(params.anchorRectInRootView).LeftTop());
        // TODO handle params.type to decide what to include in the window
        // (may be only a month, or so - but should we use a popup menu in that
        // case?)

        // FIXME we need to parse params.currentValue using other formats
        // depending on the type:
        // time: HH:mm
        // datetime: yyyy-MM-dd-THH:mmZ

        // TODO we should also handle the list of suggestions from the params
        // (probably as a BMenuField), and the min, max, and step values.

        if (params.type == InputTypeNames::datetimelocal()
                || params.type == InputTypeNames::date()
                || params.type == InputTypeNames::week()
                || params.type == InputTypeNames::month()) {
            BDateFormat format;

            m_yearControl = new BTextControl("year", NULL, NULL,
                new BMessage('yech'));
            m_yearControl->SetModificationMessage(new BMessage('yech'));
            m_yearControl->TextView()->SetMaxBytes(6);
            // TODO add filter on isdigit() only

            // Setup month menu
            BMenu* monthMenu = new BMenu("month");
            monthMenu->SetLabelFromMarked(true);

            for (int i = 1; i <= 12; i++) {
                BString out;
                format.GetMonthName(i, out);
                BMessage* message = new BMessage('moch');
                message->AddInt32("month", i);
                monthMenu->AddItem(new BMenuItem(out, message));
            }

            // Build window
            BGroupLayoutBuilder(fMainGroup)
                .AddGroup(B_VERTICAL)
                    .AddGroup(B_HORIZONTAL)
                        .Add(new BMenuField(NULL, monthMenu))
                        .Add(m_yearControl)
                    .End()
                    .Add(m_calendar = new BPrivate::BCalendarView("Date"))
                .End()
            .End();

            // Parse initial date
            BDate initialDate;

            if (params.type == InputTypeNames::date())
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-'MM'-'dd");
            else if (params.type == InputTypeNames::month()) {
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-'MM");
            } else if (params.type == InputTypeNames::week())
                format.SetDateFormat(B_LONG_DATE_FORMAT, "yyyy'-W'ww");
            format.Parse(params.currentValue, B_LONG_DATE_FORMAT, initialDate);

            m_calendar->SetDate(initialDate);

            BString out;
            format.SetDateFormat(B_SHORT_DATE_FORMAT, "yyyy");
            format.Format(out, initialDate, B_SHORT_DATE_FORMAT);
            m_yearControl->SetText(out.String());

            monthMenu->ItemAt(initialDate.Month() - 1)->SetMarked(true);

        }

        if (params.type == InputTypeNames::datetimelocal())
           fMainGroup->AddChild(new BSeparatorView(B_VERTICAL));

        if (params.type == InputTypeNames::datetimelocal()
                || params.type == InputTypeNames::time()) {
            m_hourMenu = new BMenu("hour");
            m_hourMenu->SetLabelFromMarked(true);
            m_minuteMenu = new BMenu("minute");
            m_minuteMenu->SetLabelFromMarked(true);

            // Parse the initial time
            BTimeFormat format;
            BTime initialTime;
            format.SetTimeFormat(B_SHORT_TIME_FORMAT, "HH':'mm");
            format.Parse(params.currentValue, B_SHORT_TIME_FORMAT, initialTime);

            for (int i = 0; i <= 24; i++) {
                BString label;
                label << i; // TODO we could be more locale safe here (AM/PM)
                m_hourMenu->AddItem(new BMenuItem(label, NULL));
            }

            m_hourMenu->ItemAt(initialTime.Hour())->SetMarked(true);

            for (int i = 0; i <= 60; i++) {
                BString label;
                label << i; // TODO we could be more locale safe here
                m_minuteMenu->AddItem(new BMenuItem(label, NULL));
            }

            m_minuteMenu->ItemAt(initialTime.Minute())->SetMarked(true);

            BGroupLayoutBuilder(fMainGroup)
                .AddGroup(B_VERTICAL)
                    .AddGroup(B_HORIZONTAL)
                        .Add(new BMenuField(NULL, m_hourMenu))
                        .Add(new BMenuField(NULL, m_minuteMenu))
                        .AddGlue();
        }

        // Now show only the relevant parts of the window depending on the type
        // and configure the output format
        if (params.type == InputTypeNames::month()) {
            m_format = "yyyy'-'MM";
            m_calendar->Hide();
        } else if (params.type == InputTypeNames::week()) {
            m_format = "YYYY'-W'ww";
        } else if (params.type == InputTypeNames::date()) {
            m_format = "yyyy'-'MM'-'dd";
        } else if (params.type == InputTypeNames::time()) {
            m_format = "HH':'mm";
        } else {
            // TODO datetime, datetime-local
        }
    }

    void Hide() override {
        m_client->didEndChooser();
        BWindow::Hide();
    }

    void MessageReceived(BMessage* message) {
        switch(message->what) {
            case 'done':
            {
                BString str;
                // We must use the en_US format here because this is what WebKit
                // expects.
                BLanguage language("en");
                BFormattingConventions conventions("en_US");
                if (m_calendar) {
                    // TODO handle datetime format
                    conventions.SetExplicitDateFormat(B_LONG_DATE_FORMAT, m_format);
                    BDateFormat formatter(language, conventions);
                    formatter.Format(str, m_calendar->Date(), B_LONG_DATE_FORMAT);
                } else {
                    // time-only format
                    str << m_hourMenu->Superitem()->Label();
                    if (str.Length() < 2) str.Prepend("0");
                    str << ':';
                    str << m_minuteMenu->Superitem()->Label();
                }
                m_client->didChooseValue(StringView(String(str)));
                // fallthrough
            }
            case 'canc':
                Hide();
                return;

            case 'moch':
            {
                m_calendar->SetMonth(message->FindInt32("month"));
                return;
            }

            case 'yech':
            {
                char* p;
                errno = 0;
                int year = strtol(m_yearControl->Text(), &p, 10);
                if (errno == ERANGE || year > 275759 || year <= 0
                        || p == m_yearControl->Text() || *p != '\0') {
                    m_yearControl->MarkAsInvalid(true);
                    m_okButton->SetEnabled(false); // TODO not if other fields are invalid?
                } else {
                    m_yearControl->MarkAsInvalid(false);
                    m_okButton->SetEnabled(true);
                    m_calendar->SetYear(year);
                }
                return;
            }
        }

        BWindow::MessageReceived(message);
    }

private:
    BPrivate::BCalendarView* m_calendar;
    BTextControl* m_yearControl;
    BButton* m_okButton;
    BMenu* m_hourMenu;
    BMenu* m_minuteMenu;
    BString m_format;
    BGroupView* fMainGroup;

    DateTimeChooserClient* m_client;
};

class DateTimeChooserHaiku: public DateTimeChooser
{
public:
    DateTimeChooserHaiku(DateTimeChooserClient* client)
        : m_window(new DateTimeChooserWindow(client))
    {
    }

    ~DateTimeChooserHaiku()
    {
        m_window->PostMessage(B_QUIT_REQUESTED);
    }

    void showChooser(const WebCore::DateTimeChooserParameters& params) override
    {
        m_window->Configure(params);
        m_window->Show();
    }

    void endChooser() override
    {
        m_window->Hide();
    }

private:
    DateTimeChooserWindow* m_window;
};

}


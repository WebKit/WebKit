/*
 * Copyright (C) 2014 Haiku, Inc.
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
#include "ColorChooser.h"

#include "ColorChooserClient.h"

#include <Button.h>
#include <ColorControl.h>
#include <GroupView.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <Window.h>

namespace WebCore {

class ColorSwatch: public BButton {
public:
    ColorSwatch(Color color) 
        : BButton("swatch", "", NULL) {
        SetHighColor(color);
        SetExplicitSize(BSize(16, 16));

        BMessage* message = new BMessage('pick');
        rgb_color c(color);
        message->AddData("RGBColor", B_RGB_COLOR_TYPE, &c, sizeof(c));
        SetMessage(message);
    }

    void Draw(BRect updateRect) {
        FillRect(updateRect);
        // TODO should we add a border?
    }
};

class ColorChooserWindow: public BWindow
{
public:
    ColorChooserWindow(ColorChooserClient& client)
        : BWindow(client.elementRectRelativeToRootView(), "Color Picker",
            B_FLOATING_WINDOW, B_NOT_CLOSABLE | B_NOT_RESIZABLE | B_NOT_ZOOMABLE
            | B_AUTO_UPDATE_SIZE_LIMITS)
        , m_client(client)
    {
        SetLayout(new BGroupLayout(B_VERTICAL));
        BGroupView* group = new BGroupView(B_VERTICAL);
        group->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

        Vector<Color> colors = client.suggestedColors();

        if (colors.size() > 0) {
            BGroupView* swatches = new BGroupView(B_HORIZONTAL);

            for(Color c: colors) {
                BButton* v = new ColorSwatch(c);
                swatches->AddChild(v);
            }

            group->AddChild(swatches);
        }

        control = new BColorControl(B_ORIGIN, B_CELLS_32x8, 8, "picker",
            new BMessage('chng'));
        control->SetValue(make_color(255, 255, 255));

        BButton* ok = new BButton("ok", "Done", new BMessage('done'));
        BButton* cancel = new BButton("cancel", "Cancel", new BMessage('canc'));

        m_preview = new ColorSwatch(make_color(255, 255, 255));

        BGroupLayoutBuilder(group)
            .SetInsets(5, 5, 5, 5)
            .Add(control)
            .AddGroup(B_HORIZONTAL)
                .Add(m_preview)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End()
        .End();
        AddChild(group);
    }

    void Hide() override {
        m_client.didEndChooser();
        BWindow::Hide();
    }

    void MessageReceived(BMessage* message) {
        switch(message->what) {
            case 'pick':
            {
                rgb_color* c;
                message->FindData("RGBColor", B_RGB_COLOR_TYPE, (const void**)&c, NULL);
                control->SetValue(*c);
                [[fallthrough]];
            }
            case 'chng':
            {
                m_preview->SetHighColor(control->ValueAsColor());
                m_preview->Invalidate();
                return;
            }
            case 'done':
                m_client.didChooseColor(control->ValueAsColor());
            	[[fallthrough]];
            case 'canc':
                Hide();
                return;
        }

        BWindow::MessageReceived(message);
    }

private:
    ColorChooserClient& m_client;
    BColorControl* control;
    ColorSwatch* m_preview;
};


class ColorChooserHaiku: public ColorChooser
{
public:
    ColorChooserHaiku(ColorChooserClient* client, const Color& color)
        : m_window(new ColorChooserWindow(*client))
        , m_client(*client)
    {
        reattachColorChooser(color);
    }

    ~ColorChooserHaiku()
    {
        m_window->PostMessage(B_QUIT_REQUESTED);
    }


    void reattachColorChooser(const Color& color) override
    {
        setSelectedColor(color);
        m_window->Show();
    }

    void setSelectedColor(const Color& color) override
    {
        m_client.didChooseColor(color);
    }

    void endChooser() override {
        m_window->Hide();
    }

private:
    BWindow* m_window;
    ColorChooserClient& m_client;
};

}

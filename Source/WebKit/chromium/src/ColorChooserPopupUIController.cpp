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

#include "config.h"
#include "ColorChooserPopupUIController.h"

#if ENABLE(INPUT_TYPE_COLOR) && ENABLE(PAGE_POPUP)
#include "ChromeClientImpl.h"
#include "ColorChooserClient.h"
#include "ColorSuggestionPicker.h"
#include "FrameView.h"
#include "IntRect.h"
#include "LocalizedStrings.h"
#include "PickerCommon.h"
#include "WebColorChooser.h"
#include "WebViewImpl.h"
#include <public/Platform.h>
#include <public/WebLocalizedString.h>

using namespace WebCore;

namespace WebKit {

// Keep in sync with Actions in colorSuggestionPicker.js.
enum ColorPickerPopupAction {
    ColorPickerPopupActionChooseOtherColor = -2,
    ColorPickerPopupActionCancel = -1,
    ColorPickerPopupActionSetValue = 0
};

ColorChooserPopupUIController::ColorChooserPopupUIController(ChromeClientImpl* chromeClient, ColorChooserClient* client)
    : ColorChooserUIController(chromeClient, client)
    , m_chromeClient(chromeClient)
    , m_client(client)
    , m_popup(0)
    , m_locale(Locale::createDefault())
{
}

ColorChooserPopupUIController::~ColorChooserPopupUIController()
{
}

void ColorChooserPopupUIController::openUI()
{
    if (m_client->shouldShowSuggestions())
        openPopup();
    else
        openColorChooser();
}

void ColorChooserPopupUIController::endChooser()
{
    if (m_chooser)
        m_chooser->endChooser();
    if (m_popup)
        closePopup();
}

IntSize ColorChooserPopupUIController::contentSize()
{
    return IntSize(0, 0);
}

void ColorChooserPopupUIController::writeDocument(DocumentWriter& writer)
{
    Vector<Color> suggestions = m_client->suggestions();
    Vector<String> suggestionValues;
    for (unsigned i = 0; i < suggestions.size(); i++)
        suggestionValues.append(suggestions[i].serialized());
    IntRect anchorRectInScreen = m_chromeClient->rootViewToScreen(m_client->elementRectRelativeToRootView());

    PagePopupClient::addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", writer);
    writer.addData(pickerCommonCss, sizeof(pickerCommonCss));
    writer.addData(colorSuggestionPickerCss, sizeof(colorSuggestionPickerCss));
    PagePopupClient::addString("</style></head><body><div id=main>Loading...</div><script>\n"
        "window.dialogArguments = {\n", writer);
    PagePopupClient::addProperty("values", suggestionValues, writer);
    PagePopupClient::addProperty("otherColorLabel", Platform::current()->queryLocalizedString(WebLocalizedString::OtherColorLabel), writer);
    addProperty("anchorRectInScreen", anchorRectInScreen, writer);
    PagePopupClient::addString("};\n", writer);
    writer.addData(pickerCommonJs, sizeof(pickerCommonJs));
    writer.addData(colorSuggestionPickerJs, sizeof(colorSuggestionPickerJs));
    PagePopupClient::addString("</script></body>\n", writer);
}

Locale& ColorChooserPopupUIController::locale()
{
    return *m_locale;
}

void ColorChooserPopupUIController::setValueAndClosePopup(int numValue, const String& stringValue)
{
    ASSERT(m_popup);
    ASSERT(m_client);
    if (numValue == ColorPickerPopupActionSetValue)
        m_client->didChooseColor(Color(stringValue));
    if (numValue == ColorPickerPopupActionChooseOtherColor)
        openColorChooser();
    closePopup();
}

void ColorChooserPopupUIController::didClosePopup()
{
    m_popup = 0;

    if (!m_chooser)
        didEndChooser();
}


void ColorChooserPopupUIController::openPopup()
{
    ASSERT(!m_popup);
    m_popup = m_chromeClient->openPagePopup(this, m_client->elementRectRelativeToRootView());
}

void ColorChooserPopupUIController::closePopup()
{
    if (!m_popup)
        return;
    m_chromeClient->closePagePopup(m_popup);
}

}
#endif // ENABLE(INPUT_TYPE_COLOR) && ENABLE(PAGE_POPUP)

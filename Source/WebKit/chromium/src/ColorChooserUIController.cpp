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
#include "ColorChooserUIController.h"

#if ENABLE(INPUT_TYPE_COLOR)

#include "ChromeClientImpl.h"
#include "Color.h"
#include "ColorChooserClient.h"
#include "ColorSuggestionPicker.h"
#include "IntRect.h"
#include "LocalizedStrings.h"
#include "PickerCommon.h"
#include "WebColorChooser.h"
#include "platform/WebColor.h"
#include "platform/WebKitPlatformSupport.h"
#include <public/WebLocalizedString.h>

namespace WebKit {

// Keep in sync with Actions in colorSuggestionPicker.js.
enum ColorPickerPopupAction {
    ColorPickerPopupActionChooseOtherColor = -2,
    ColorPickerPopupActionCancel = -1,
    ColorPickerPopupActionSetValue = 0
};

ColorChooserUIController::ColorChooserUIController(ChromeClientImpl* chromeClient, WebCore::ColorChooserClient* client)
    : m_chromeClient(chromeClient)
    , m_client(client)
    , m_popup(0)
{
    if (m_client->shouldShowSuggestions())
        openPopup();
    else
        openColorChooser();
}

ColorChooserUIController::~ColorChooserUIController()
{
}

void ColorChooserUIController::setSelectedColor(const WebCore::Color& color)
{
    ASSERT(m_chooser);
    m_chooser->setSelectedColor(static_cast<WebColor>(color.rgb()));
}

void ColorChooserUIController::endChooser()
{
    if (m_chooser)
        m_chooser->endChooser();
    if (m_popup)
        closePopup();
}

void ColorChooserUIController::didChooseColor(const WebColor& color)
{
    ASSERT(m_client);
    m_client->didChooseColor(WebCore::Color(static_cast<WebCore::RGBA32>(color)));
}

void ColorChooserUIController::didEndChooser()
{
    ASSERT(m_client);
    m_chooser = nullptr;
    m_client->didEndChooser();
}

WebCore::IntSize ColorChooserUIController::contentSize()
{
    return WebCore::IntSize(0, 0);
}

void ColorChooserUIController::writeDocument(WebCore::DocumentWriter& writer)
{
    Vector<WebCore::Color> suggestions = m_client->suggestions();
    Vector<String> suggestionValues;
    for (unsigned i = 0; i < suggestions.size(); i++)
        suggestionValues.append(suggestions[i].serialized());
    
    WebCore::PagePopupClient::addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", writer);
    writer.addData(WebCore::pickerCommonCss, sizeof(WebCore::pickerCommonCss));
    writer.addData(WebCore::colorSuggestionPickerCss, sizeof(WebCore::colorSuggestionPickerCss));
    WebCore::PagePopupClient::addString("</style></head><body><div id=main>Loading...</div><script>\n"
                                      "window.dialogArguments = {\n", writer);
    WebCore::PagePopupClient::addProperty("values", suggestionValues, writer);       
    WebCore::PagePopupClient::addProperty("otherColorLabel", Platform::current()->queryLocalizedString(WebLocalizedString::OtherColorLabel), writer);
    WebCore::PagePopupClient::addString("};\n", writer);
    writer.addData(WebCore::pickerCommonJs, sizeof(WebCore::pickerCommonJs));
    writer.addData(WebCore::colorSuggestionPickerJs, sizeof(WebCore::colorSuggestionPickerJs));
    WebCore::PagePopupClient::addString("</script></body>\n", writer);
}

void ColorChooserUIController::setValueAndClosePopup(int numValue, const String& stringValue)
{
    ASSERT(m_popup);
    ASSERT(m_client);
    if (numValue == ColorPickerPopupActionSetValue)
        m_client->didChooseColor(WebCore::Color(stringValue));
    if (numValue == ColorPickerPopupActionChooseOtherColor)
        openColorChooser();
    closePopup();
}

void ColorChooserUIController::didClosePopup()
{
    m_popup = 0;

    if (!m_chooser)
        didEndChooser();
}

void ColorChooserUIController::openPopup()
{
    ASSERT(!m_popup);
    m_popup = m_chromeClient->openPagePopup(this, m_client->elementRectRelativeToRootView());
}

void ColorChooserUIController::closePopup()
{
    if (!m_popup)
        return;
    m_chromeClient->closePagePopup(m_popup);
}

void ColorChooserUIController::openColorChooser()
{
    ASSERT(!m_chooser);
    m_chooser = m_chromeClient->createWebColorChooser(this, static_cast<WebColor>(m_client->currentColor().rgb()));
}

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)

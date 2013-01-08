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

#include "config.h"
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeChooserImpl.h"

#include "CalendarPicker.h"
#include "ChromeClientImpl.h"
#include "DateComponents.h"
#include "DateTimeChooserClient.h"
#include "FrameView.h"
#include "InputTypeNames.h"
#include "Language.h"
#include "NotImplemented.h"
#include "PickerCommon.h"
#include "PlatformLocale.h"
#include "RenderTheme.h"
#include "WebViewImpl.h"
#include <public/Platform.h>
#include <public/WebLocalizedString.h>

#if !ENABLE(CALENDAR_PICKER)
#error "ENABLE_INPUT_MULTIPLE_FIELDS_UI requires ENABLE_CALENDAR_PICKER in Chromium."
#endif

using namespace WebCore;

namespace WebKit {

DateTimeChooserImpl::DateTimeChooserImpl(ChromeClientImpl* chromeClient, WebCore::DateTimeChooserClient* client, const WebCore::DateTimeChooserParameters& parameters)
    : m_chromeClient(chromeClient)
    , m_client(client)
    , m_popup(0)
    , m_parameters(parameters)
    , m_locale(WebCore::Locale::create(parameters.locale))
{
    ASSERT(m_chromeClient);
    ASSERT(m_client);
    m_popup = m_chromeClient->openPagePopup(this, m_parameters.anchorRectInRootView);
}

PassRefPtr<DateTimeChooserImpl> DateTimeChooserImpl::create(ChromeClientImpl* chromeClient, WebCore::DateTimeChooserClient* client, const WebCore::DateTimeChooserParameters& parameters)
{
    return adoptRef(new DateTimeChooserImpl(chromeClient, client, parameters));
}

DateTimeChooserImpl::~DateTimeChooserImpl()
{
}

void DateTimeChooserImpl::endChooser()
{
    if (!m_popup)
        return;
    m_chromeClient->closePagePopup(m_popup);
}

WebCore::IntSize DateTimeChooserImpl::contentSize()
{
    return WebCore::IntSize(0, 0);
}

void DateTimeChooserImpl::writeDocument(WebCore::DocumentWriter& writer)
{
    WebCore::DateComponents minDate;
    WebCore::DateComponents maxDate;
    if (m_parameters.type == WebCore::InputTypeNames::month()) {
        minDate.setMonthsSinceEpoch(m_parameters.minimum);
        maxDate.setMonthsSinceEpoch(m_parameters.maximum);
    } else if (m_parameters.type == WebCore::InputTypeNames::week()) {
        minDate.setMillisecondsSinceEpochForWeek(m_parameters.minimum);
        maxDate.setMillisecondsSinceEpochForWeek(m_parameters.maximum);
    } else {
        minDate.setMillisecondsSinceEpochForDate(m_parameters.minimum);
        maxDate.setMillisecondsSinceEpochForDate(m_parameters.maximum);
    }
    String stepString = String::number(m_parameters.step);
    String stepBaseString = String::number(m_parameters.stepBase, 11, WTF::TruncateTrailingZeros);
    IntRect anchorRectInScreen = m_chromeClient->rootViewToScreen(m_parameters.anchorRectInRootView);
    String todayLabelString;
    String otherDateLabelString;
    if (m_parameters.type == WebCore::InputTypeNames::month()) {
        todayLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::ThisMonthButtonLabel);
        otherDateLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::OtherMonthLabel);
    } else if (m_parameters.type == WebCore::InputTypeNames::week()) {
        todayLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::ThisWeekButtonLabel);
        otherDateLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::OtherWeekLabel);
    } else {
        todayLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::CalendarToday);
        otherDateLabelString = Platform::current()->queryLocalizedString(WebLocalizedString::OtherDateLabel);
    }

    addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", writer);
    writer.addData(WebCore::pickerCommonCss, sizeof(WebCore::pickerCommonCss));
    writer.addData(WebCore::suggestionPickerCss, sizeof(WebCore::suggestionPickerCss));
    writer.addData(WebCore::calendarPickerCss, sizeof(WebCore::calendarPickerCss));
    CString extraStyle = WebCore::RenderTheme::defaultTheme()->extraCalendarPickerStyleSheet();
    if (extraStyle.length())
        writer.addData(extraStyle.data(), extraStyle.length());
    addString("</style></head><body><div id=main>Loading...</div><script>\n"
               "window.dialogArguments = {\n", writer);
    addProperty("anchorRectInScreen", anchorRectInScreen, writer);
    addProperty("min", minDate.toString(), writer);
    addProperty("max", maxDate.toString(), writer);
    addProperty("step", stepString, writer);
    addProperty("stepBase", stepBaseString, writer);
    addProperty("required", m_parameters.required, writer);
    addProperty("currentValue", m_parameters.currentValue, writer);
    addProperty("locale", m_parameters.locale.string(), writer);
    addProperty("todayLabel", todayLabelString, writer);
    addProperty("clearLabel", Platform::current()->queryLocalizedString(WebLocalizedString::CalendarClear), writer);
    addProperty("weekLabel", Platform::current()->queryLocalizedString(WebLocalizedString::WeekNumberLabel), writer);
    addProperty("weekStartDay", m_locale->firstDayOfWeek(), writer);
    addProperty("dayLabels", m_locale->weekDayShortLabels(), writer);
    addProperty("isLocaleRTL", m_locale->isRTL(), writer);
    addProperty("isRTL", m_parameters.isAnchorElementRTL, writer);
    addProperty("mode", m_parameters.type.string(), writer);
    if (m_parameters.suggestionValues.size()) {
        addProperty("inputWidth", static_cast<unsigned>(m_parameters.anchorRectInRootView.width()), writer);
        addProperty("suggestionValues", m_parameters.suggestionValues, writer);
        addProperty("localizedSuggestionValues", m_parameters.localizedSuggestionValues, writer);
        addProperty("suggestionLabels", m_parameters.suggestionLabels, writer);
        addProperty("showOtherDateEntry", WebCore::RenderTheme::defaultTheme()->supportsCalendarPicker(m_parameters.type), writer);
        addProperty("otherDateLabel", otherDateLabelString, writer);
        addProperty("suggestionHighlightColor", WebCore::RenderTheme::defaultTheme()->activeListBoxSelectionBackgroundColor().serialized(), writer);
        addProperty("suggestionHighlightTextColor", WebCore::RenderTheme::defaultTheme()->activeListBoxSelectionForegroundColor().serialized(), writer);
    }
    addString("}\n", writer);

    writer.addData(WebCore::pickerCommonJs, sizeof(WebCore::pickerCommonJs));
    writer.addData(WebCore::suggestionPickerJs, sizeof(WebCore::suggestionPickerJs));
    writer.addData(WebCore::calendarPickerJs, sizeof(WebCore::calendarPickerJs));
    addString("</script></body>\n", writer);
}

WebCore::Locale& DateTimeChooserImpl::locale()
{
    return *m_locale;
}

void DateTimeChooserImpl::setValueAndClosePopup(int numValue, const String& stringValue)
{
    RefPtr<DateTimeChooserImpl> protector(this);
    if (numValue >= 0)
        m_client->didChooseValue(stringValue);
    endChooser();
}

void DateTimeChooserImpl::didClosePopup()
{
    ASSERT(m_client);
    m_popup = 0;
    m_client->didEndChooser();
}

} // namespace WebKit

#endif // ENABLE(INPUT_MULTIPLE_FIELDS_UI)

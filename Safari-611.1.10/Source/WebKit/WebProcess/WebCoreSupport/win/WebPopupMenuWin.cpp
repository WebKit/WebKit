/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPopupMenu.h"

#include "PlatformPopupMenuData.h"
#include <WebCore/LengthFunctions.h>
#include <WebCore/PopupMenuClient.h>
#include <WebCore/RenderTheme.h>

namespace WebKit {
using namespace WebCore;

static const int separatorPadding = 4;
static const int separatorHeight = 1;
static const int popupWindowBorderWidth = 1;

void WebPopupMenu::setUpPlatformData(const WebCore::IntRect& pageCoordinates, PlatformPopupMenuData& data)
{
    int itemCount = m_popupClient->listSize();

    auto font = m_popupClient->menuStyle().font();

    data.m_clientPaddingLeft = m_popupClient->clientPaddingLeft();
    data.m_clientPaddingRight = m_popupClient->clientPaddingRight();
    data.m_clientInsetLeft = m_popupClient->clientInsetLeft();
    data.m_clientInsetRight = m_popupClient->clientInsetRight();
    data.m_itemHeight = font.fontMetrics().height() + 1;

    int popupWidth = 0;
    for (size_t i = 0; i < itemCount; ++i) {
        String text = m_popupClient->itemText(i);
        if (text.isEmpty())
            continue;

        auto itemFontCascade = font;
        if (m_popupClient->itemIsLabel(i)) {
            auto d = itemFontCascade.fontDescription();
            d.setWeight(boldWeightValue());
            itemFontCascade = FontCascade(WTFMove(d), itemFontCascade.letterSpacing(), itemFontCascade.wordSpacing());
            itemFontCascade.update(m_popupClient->fontSelector());
        }

        popupWidth = std::max<float>(popupWidth, ceilf(itemFontCascade.width(TextRun(text))));
    }

    // FIXME: popupWidth should probably take into account monitor constraints as is done with WebPopupMenuProxyWin::calculatePositionAndSize.
    popupWidth += std::max(0, data.m_clientPaddingRight - data.m_clientInsetRight) + std::max(0, data.m_clientPaddingLeft - data.m_clientInsetLeft);
    popupWidth += 2 * popupWindowBorderWidth;
    data.m_popupWidth = popupWidth;

    // The backing stores should be drawn at least as wide as the control on the page to match the width of the popup window we'll create.
    int backingStoreWidth = std::max(pageCoordinates.width() - m_popupClient->clientInsetLeft() - m_popupClient->clientInsetRight(), popupWidth);

    IntSize backingStoreSize(backingStoreWidth, (itemCount * data.m_itemHeight));
    data.m_notSelectedBackingStore = ShareableBitmap::createShareable(backingStoreSize, { });
    data.m_selectedBackingStore = ShareableBitmap::createShareable(backingStoreSize, { });

    std::unique_ptr<GraphicsContext> notSelectedBackingStoreContext = data.m_notSelectedBackingStore->createGraphicsContext();
    std::unique_ptr<GraphicsContext> selectedBackingStoreContext = data.m_selectedBackingStore->createGraphicsContext();

    Color activeOptionBackgroundColor = RenderTheme::singleton().activeListBoxSelectionBackgroundColor({ });
    Color activeOptionTextColor = RenderTheme::singleton().activeListBoxSelectionForegroundColor({ });

    for (int y = 0; y < backingStoreSize.height(); y += data.m_itemHeight) {
        int index = y / data.m_itemHeight;

        PopupMenuStyle itemStyle = m_popupClient->itemStyle(index);

        Color optionBackgroundColor = itemStyle.backgroundColor();
        Color optionTextColor = itemStyle.foregroundColor();

        IntRect itemRect(0, y, backingStoreWidth, data.m_itemHeight);

        // Draw the background for this menu item
        if (itemStyle.isVisible()) {
            notSelectedBackingStoreContext->fillRect(itemRect, optionBackgroundColor);
            selectedBackingStoreContext->fillRect(itemRect, activeOptionBackgroundColor);
        }

        if (m_popupClient->itemIsSeparator(index)) {
            IntRect separatorRect(itemRect.x() + separatorPadding, itemRect.y() + (itemRect.height() - separatorHeight) / 2, itemRect.width() - 2 * separatorPadding, separatorHeight);

            notSelectedBackingStoreContext->fillRect(separatorRect, optionTextColor);
            selectedBackingStoreContext->fillRect(separatorRect, activeOptionTextColor);
            continue;
        }

        String itemText = m_popupClient->itemText(index);

        TextRun textRun(itemText, 0, 0, AllowRightExpansion, itemStyle.textDirection(), itemStyle.hasTextDirectionOverride());

        notSelectedBackingStoreContext->setFillColor(optionTextColor);
        selectedBackingStoreContext->setFillColor(activeOptionTextColor);

        auto itemFontCascade = font;
        if (m_popupClient->itemIsLabel(index)) {
            auto d = itemFontCascade.fontDescription();
            d.setWeight(boldWeightValue());
            itemFontCascade = FontCascade(WTFMove(d), itemFontCascade.letterSpacing(), itemFontCascade.wordSpacing());
            itemFontCascade.update(m_popupClient->fontSelector());
        }

        // Draw the item text
        if (itemStyle.isVisible()) {
            int textX = 0;
            if (m_popupClient->menuStyle().textDirection() == TextDirection::LTR) {
                textX = std::max<int>(0, m_popupClient->clientPaddingLeft() - m_popupClient->clientInsetLeft());
                if (RenderTheme::singleton().popupOptionSupportsTextIndent())
                    textX += minimumIntValueForLength(itemStyle.textIndent(), itemRect.width());
            } else {
                textX = itemRect.width() - m_popupClient->menuStyle().font().width(textRun);
                textX = std::min<int>(textX, textX - m_popupClient->clientPaddingRight() + m_popupClient->clientInsetRight());
                if (RenderTheme::singleton().popupOptionSupportsTextIndent())
                    textX -= minimumIntValueForLength(itemStyle.textIndent(), itemRect.width());
            }
            int textY = itemRect.y() + itemFontCascade.fontMetrics().ascent() + (itemRect.height() - itemFontCascade.fontMetrics().height()) / 2;

            notSelectedBackingStoreContext->drawBidiText(itemFontCascade, textRun, IntPoint(textX, textY));
            selectedBackingStoreContext->drawBidiText(itemFontCascade, textRun, IntPoint(textX, textY));
        }
    }
}

} // namespace WebKit

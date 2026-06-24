/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FallbackPopupMenu.h"

#include "AbortSignal.h"
#include "CSSMarkup.h"
#include "CSSPropertyNames.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "ColorSerialization.h"
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FontCascade.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "MouseEvent.h"
#include "Node.h"
#include "NodeDocument.h"
#include "PopupMenuStyle.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "UserAgentParts.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FallbackPopupMenu);

class FallbackPopupMenuItemEventListener final : public EventListener {
public:
    static Ref<FallbackPopupMenuItemEventListener> create(FallbackPopupMenu& menu, int listIndex)
    {
        return adoptRef(*new FallbackPopupMenuItemEventListener(menu, listIndex));
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        if (event.type() == eventNames().mouseupEvent) {
            if (RefPtr menu = m_menu.get())
                menu->itemClicked(m_listIndex);
        }
    }

private:
    FallbackPopupMenuItemEventListener(FallbackPopupMenu& menu, int listIndex)
        : EventListener(CPPEventListenerType)
        , m_menu(menu)
        , m_listIndex(listIndex)
    {
    }

    WeakPtr<FallbackPopupMenu> m_menu;
    int m_listIndex;
};

static void applyFontCascade(HTMLElement& element, const FontCascade& font)
{
    element.setInlineStyleProperty(CSSPropertyFontSize, font.size(), CSSUnitType::CSS_PX);
    element.setInlineStyleProperty(CSSPropertyFontWeight, static_cast<double>(font.weight()), CSSUnitType::CSS_NUMBER);
    if (font.fontStyleSlope())
        element.setInlineStyleProperty(CSSPropertyFontStyle, CSSValueItalic);

    StringBuilder families;
    for (unsigned i = 0; i < font.familyCount(); ++i) {
        if (i)
            families.append(", "_s);
        families.append(serializeFontFamily(font.familyAt(i).name));
    }
    if (!families.isEmpty())
        element.setInlineStyleProperty(CSSPropertyFontFamily, families.toString());
}

static void applyItemStyle(HTMLElement& item, const PopupMenuStyle& style)
{
    if (auto& foreground = style.foregroundColor(); foreground.isValid())
        item.setInlineStyleProperty(CSSPropertyColor, serializationForCSS(foreground));

    if (style.backgroundColorType() == PopupMenuStyle::CustomBackgroundColor) {
        if (auto& background = style.backgroundColor(); background.isValid())
            item.setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForCSS(background));
    }

    if (auto& language = style.language(); !language.isEmpty())
        item.setAttributeWithoutSynchronization(HTMLNames::langAttr, AtomString { language });

    if (CheckedRef font = style.font())
        applyFontCascade(item, font);

    item.setInlineStyleProperty(CSSPropertyDirection, style.textDirection() == TextDirection::RTL ? CSSValueRtl : CSSValueLtr);
    if (style.hasTextDirectionOverride())
        item.setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueBidiOverride);
}

static void applyMenuStyle(HTMLElement& container, const PopupMenuStyle& style)
{
    if (auto& foreground = style.foregroundColor(); foreground.isValid())
        container.setInlineStyleProperty(CSSPropertyColor, serializationForCSS(foreground));

    if (style.backgroundColorType() == PopupMenuStyle::CustomBackgroundColor) {
        if (auto& background = style.backgroundColor(); background.isValid())
            container.setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForCSS(background));
    }

    if (CheckedRef font = style.font())
        applyFontCascade(container, font);

    container.setInlineStyleProperty(CSSPropertyDirection, style.textDirection() == TextDirection::RTL ? CSSValueRtl : CSSValueLtr);
    if (style.hasTextDirectionOverride())
        container.setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueBidiOverride);
}

class FallbackPopupMenuDismissListener final : public EventListener {
public:
    static Ref<FallbackPopupMenuDismissListener> create(FallbackPopupMenu& menu)
    {
        return adoptRef(*new FallbackPopupMenuDismissListener(menu));
    }

    void handleEvent(ScriptExecutionContext&, Event& event) final
    {
        RefPtr menu = m_menu.get();
        if (!menu)
            return;
        if (event.type() == eventNames().scrollEvent || event.type() == eventNames().resizeEvent) {
            menu->hide();
            return;
        }
        if (event.type() == eventNames().keydownEvent) {
            if (auto* keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
                auto& key = keyboardEvent->key();
                if (key == "Enter"_s || key == "Escape"_s) {
                    menu->hide();
                    event.setDefaultHandled();
                }
            }
            return;
        }
        RefPtr target = dynamicDowncast<Node>(event.target());
        if (!target)
            return;
        menu->maybeClose(*target, event);
    }

private:
    explicit FallbackPopupMenuDismissListener(FallbackPopupMenu& menu)
        : EventListener(CPPEventListenerType)
        , m_menu(menu)
    {
    }

    WeakPtr<FallbackPopupMenu> m_menu;
};

FallbackPopupMenu::FallbackPopupMenu(HTMLSelectElement& element)
    : m_element(element)
    , m_updateTimer(RunLoop::mainSingleton(), "FallbackPopupMenu update timer"_s, this, &FallbackPopupMenu::updateFromElementTimerFired)
{
}

FallbackPopupMenu::~FallbackPopupMenu()
{
    deletePopupTree();
}

void FallbackPopupMenu::show(const IntRect& rect, LocalFrameView& frameView, int)
{
    if (m_container)
        return;
    buildPopupTree(rect, frameView);
}

void FallbackPopupMenu::hide()
{
    deletePopupTree();
    if (RefPtr element = m_element.get()) {
        element->setActive(false);
        element->popupDidHide();
    }
}

void FallbackPopupMenu::updateFromElement()
{
    if (!m_container)
        return;
    RefPtr element = m_element.get();
    if (!element || !element->renderer())
        return;
    if (element->document().inStyleRecalc()) {
        m_updateTimer.startOneShot(0_s);
        return;
    }
    RefPtr view = element->document().view();
    if (!view)
        return;
    deletePopupTree();
    if (CheckedPtr renderer = element->renderer())
        buildPopupTree(renderer->absoluteBoundingBoxRect(), *view);
}

void FallbackPopupMenu::updateFromElementTimerFired()
{
    updateFromElement();
}

void FallbackPopupMenu::disconnectClient()
{
    deletePopupTree();
    m_element = nullptr;
}

void FallbackPopupMenu::itemClicked(int listIndex)
{
    RefPtr element = m_element.get();
    if (!element || element->itemIsLabel(listIndex))
        return;
    Ref protectedElement = *element;
    element->valueChanged(listIndex);
    hide();
}

void FallbackPopupMenu::maybeClose(Node& target, Event& event)
{
    if (RefPtr element = m_element.get()) {
        if (element.get() == &target || element->contains(&target)) {
            // Shadow DOM retargets clicks on popup items so they appear with target == SELECT at the document level.
            // Use coordinates to distinguish when a popup item click lands outside the SELECT's bounding rect.
            event.setDefaultHandled();
            auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
            CheckedPtr renderer = element->renderer();
            if (mouseEvent && renderer) {
                IntPoint clickPos(mouseEvent->pageX(), mouseEvent->pageY());
                if (!renderer->absoluteBoundingBoxRect().contains(clickPos))
                    return; // Don't hide.
            }
        }
    }
    hide();
}

void FallbackPopupMenu::buildPopupTree(const IntRect& elementRect, LocalFrameView& frameView)
{
    RefPtr element = m_element.get();
    if (!element)
        return;

    auto scrollX = frameView.scrollX();
    auto scrollY = frameView.scrollY();
    auto viewportHeight = frameView.visibleContentRect().height();

    Ref shadowRoot = element->ensureUserAgentShadowRoot();

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    ScriptDisallowedScope::EventAllowedScope allowedScope(shadowRoot);

    Ref document = element->document();

    Ref container = HTMLDivElement::create(document.get());
    shadowRoot->appendChild(container);
    container->setUserAgentPart(UserAgentParts::internalFallbackPopupMenu());
    applyMenuStyle(container, element->menuStyle());
    container->setInlineStyleProperty(CSSPropertyPosition, CSSValueFixed);
    container->setInlineStyleProperty(CSSPropertyLeft, elementRect.x() - scrollX, CSSUnitType::CSS_PX);
    container->setInlineStyleProperty(CSSPropertyWidth, elementRect.width(), CSSUnitType::CSS_PX);

    if (viewportHeight) {
        // There are some heuristics here to try and position the popup in a pleasing way.
        // Unlike a native popup we cannot grow past the viewport.
        // Our goal is to prefer showing the popup below the element when possible.
        // Once there is not enough space we flip it to show above the element.
        // No matter what the maximum size is half of the viewport.
        auto halfViewport = viewportHeight / 2;
        auto elementTop = elementRect.y() - scrollY;
        auto elementBottom = elementRect.maxY() - scrollY;
        auto spaceBelow = std::max(0, viewportHeight - elementBottom);
        auto spaceAbove = std::max(0, elementTop);
        static constexpr int minSpaceBelow = 96;
        static constexpr int popupPadding = 12; // It doesn't look nice to touch the edges of the viewport.
        if (spaceBelow < minSpaceBelow) {
            container->setInlineStyleProperty(CSSPropertyBottom, viewportHeight - elementTop, CSSUnitType::CSS_PX);
            container->setInlineStyleProperty(CSSPropertyMaxHeight, std::min(spaceAbove - popupPadding, halfViewport), CSSUnitType::CSS_PX);
        } else {
            container->setInlineStyleProperty(CSSPropertyTop, elementBottom, CSSUnitType::CSS_PX);
            container->setInlineStyleProperty(CSSPropertyMaxHeight, std::min(spaceBelow - popupPadding, halfViewport), CSSUnitType::CSS_PX);
        }
    } else
        container->setInlineStyleProperty(CSSPropertyTop, elementRect.maxY() - scrollY, CSSUnitType::CSS_PX);

    int size = element->listSize();
    for (int i = 0; i < size; ++i) {
        if (element->itemIsSeparator(i)) {
            Ref item = HTMLDivElement::create(document.get());
            container->appendChild(item);
            item->setUserAgentPart(UserAgentParts::internalFallbackPopupMenuSeparator());
            continue;
        }

        auto style = element->itemStyle(i);
        if (style.isDisplayNone() || !style.isVisible())
            continue;

        Ref item = HTMLDivElement::create(document.get());
        container->appendChild(item);

        if (element->itemIsLabel(i))
            item->setUserAgentPart(UserAgentParts::internalFallbackPopupMenuGroupLabel());
        else {
            bool selected = element->itemIsSelected(i);
            item->setUserAgentPart(selected ? UserAgentParts::internalFallbackPopupMenuItemSelected() : UserAgentParts::internalFallbackPopupMenuItem());
            Ref itemListener = FallbackPopupMenuItemEventListener::create(*this, i);
            item->addEventListener(eventNames().mouseupEvent, WTF::move(itemListener));
        }

        if (auto accessibilityText = element->itemAccessibilityText(i); !accessibilityText.isEmpty())
            item->setAttributeWithoutSynchronization(HTMLNames::aria_labelAttr, AtomString { accessibilityText });

        if (auto toolTip = element->itemToolTip(i); !toolTip.isEmpty())
            item->setAttributeWithoutSynchronization(HTMLNames::titleAttr, AtomString { toolTip });

        applyItemStyle(item, style);

        item->appendChild(Text::create(document.get(), element->itemText(i)));
    }
    m_container = WTF::move(container);

    Ref dismissListener = FallbackPopupMenuDismissListener::create(*this);
    AddEventListenerOptions captureOptions;
    captureOptions.capture = true;
    document->addEventListener(eventNames().mousedownEvent, dismissListener, captureOptions);
    document->addEventListener(eventNames().keydownEvent, dismissListener, captureOptions);
    document->addEventListener(eventNames().scrollEvent, dismissListener, captureOptions);
    if (RefPtr window = document->window())
        window->addEventListener(eventNames().resizeEvent, dismissListener, captureOptions);
    m_dismissListener = WTF::move(dismissListener);
}

void FallbackPopupMenu::deletePopupTree()
{
    if (m_dismissListener) {
        Ref dismissListener = m_dismissListener.releaseNonNull();
        if (RefPtr element = m_element.get()) {
            Ref document = element->document();
            EventListenerOptions captureOptions;
            captureOptions.capture = true;
            document->removeEventListener(eventNames().mousedownEvent, dismissListener, captureOptions);
            document->removeEventListener(eventNames().keydownEvent, dismissListener, captureOptions);
            document->removeEventListener(eventNames().scrollEvent, dismissListener, captureOptions);
            if (RefPtr window = element->document().window())
                window->removeEventListener(eventNames().resizeEvent, dismissListener, captureOptions);
        }
    }

    if (m_container) {
        Ref container = m_container.releaseNonNull();
        if (RefPtr element = m_element.get()) {
            if (RefPtr shadowRoot = element->userAgentShadowRoot()) {
                ScriptDisallowedScope::EventAllowedScope allowedScope(*shadowRoot);
                shadowRoot->removeChild(container);
            }
        }
    }
}

} // namespace WebCore

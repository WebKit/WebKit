/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderThemeGadget.h"

#if GTK_CHECK_VERSION(3, 20, 0)

#include "FloatRect.h"
#include "GRefPtrGtk.h"

namespace WebCore {

std::unique_ptr<RenderThemeGadget> RenderThemeGadget::create(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
{
    switch (info.type) {
    case RenderThemeGadget::Type::Generic:
        return std::make_unique<RenderThemeGadget>(info, parent, siblings, position);
    case RenderThemeGadget::Type::TextField:
        return std::make_unique<RenderThemeTextFieldGadget>(info, parent, siblings, position);
    case RenderThemeGadget::Type::Radio:
    case RenderThemeGadget::Type::Check:
        return std::make_unique<RenderThemeToggleGadget>(info, parent, siblings, position);
    case RenderThemeGadget::Type::Arrow:
        return std::make_unique<RenderThemeArrowGadget>(info, parent, siblings, position);
    case RenderThemeGadget::Type::Icon:
        return std::make_unique<RenderThemeIconGadget>(info, parent, siblings, position);
    case RenderThemeGadget::Type::Scrollbar:
        return std::make_unique<RenderThemeScrollbarGadget>(info, parent, siblings, position);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static GRefPtr<GtkStyleContext> createStyleContext(GtkWidgetPath* path, GtkStyleContext* parent)
{
    GRefPtr<GtkStyleContext> context = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(context.get(), path);
    gtk_style_context_set_parent(context.get(), parent);
    // Unfortunately, we have to explicitly set the state again here for it to take effect.
    gtk_style_context_set_state(context.get(), gtk_widget_path_iter_get_state(path, -1));
    return context;
}

static void appendElementToPath(GtkWidgetPath* path, const RenderThemeGadget::Info& info)
{
    // Scrollbars need to use its GType to be able to get non-CSS style properties.
    gtk_widget_path_append_type(path, info.type == RenderThemeGadget::Type::Scrollbar ? GTK_TYPE_SCROLLBAR : G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name(path, -1, info.name);
    for (const auto* className : info.classList)
        gtk_widget_path_iter_add_class(path, -1, className);
    gtk_widget_path_iter_set_state(path, -1, static_cast<GtkStateFlags>(gtk_widget_path_iter_get_state(path, -1) | info.state));
}

RenderThemeGadget::RenderThemeGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
{
    GRefPtr<GtkWidgetPath> path = parent ? adoptGRef(gtk_widget_path_copy(gtk_style_context_get_path(parent->context()))) : adoptGRef(gtk_widget_path_new());
    if (!siblings.isEmpty()) {
        GRefPtr<GtkWidgetPath> siblingsPath = adoptGRef(gtk_widget_path_new());
        for (const auto& siblingInfo : siblings)
            appendElementToPath(siblingsPath.get(), siblingInfo);
        gtk_widget_path_append_with_siblings(path.get(), siblingsPath.get(), position);
    } else
        appendElementToPath(path.get(), info);
    m_context = createStyleContext(path.get(), parent ? parent->context() : nullptr);
}

RenderThemeGadget::~RenderThemeGadget()
{
}

GtkBorder RenderThemeGadget::marginBox() const
{
    GtkBorder returnValue;
    gtk_style_context_get_margin(m_context.get(), gtk_style_context_get_state(m_context.get()), &returnValue);
    return returnValue;
}

GtkBorder RenderThemeGadget::borderBox() const
{
    GtkBorder returnValue;
    gtk_style_context_get_border(m_context.get(), gtk_style_context_get_state(m_context.get()), &returnValue);
    return returnValue;
}

GtkBorder RenderThemeGadget::paddingBox() const
{
    GtkBorder returnValue;
    gtk_style_context_get_padding(m_context.get(), gtk_style_context_get_state(m_context.get()), &returnValue);
    return returnValue;
}

GtkBorder RenderThemeGadget::contentsBox() const
{
    auto margin = marginBox();
    auto border = borderBox();
    auto padding = paddingBox();
    padding.left += margin.left + border.left;
    padding.right += margin.right + border.right;
    padding.top += margin.top + border.top;
    padding.bottom += margin.bottom + border.bottom;
    return padding;
}

Color RenderThemeGadget::color() const
{
    GdkRGBA returnValue;
    gtk_style_context_get_color(m_context.get(), gtk_style_context_get_state(m_context.get()), &returnValue);
    return returnValue;
}

Color RenderThemeGadget::backgroundColor() const
{
    GdkRGBA returnValue;
    gtk_style_context_get_background_color(m_context.get(), gtk_style_context_get_state(m_context.get()), &returnValue);
    return returnValue;
}

double RenderThemeGadget::opacity() const
{
    double returnValue;
    gtk_style_context_get(m_context.get(), gtk_style_context_get_state(m_context.get()), "opacity", &returnValue, nullptr);
    return returnValue;
}

IntSize RenderThemeGadget::minimumSize() const
{
    int width, height;
    gtk_style_context_get(m_context.get(), gtk_style_context_get_state(m_context.get()), "min-width", &width, "min-height", &height, nullptr);
    return IntSize(width, height);
}

IntSize RenderThemeGadget::preferredSize() const
{
    auto margin = marginBox();
    auto border = borderBox();
    auto padding = paddingBox();
    auto minSize = minimumSize();
    minSize.expand(margin.left + margin.right + border.left + border.right + padding.left + padding.right,
        margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom);
    return minSize;
}

bool RenderThemeGadget::render(cairo_t* cr, const FloatRect& paintRect, FloatRect* contentsRect)
{
    FloatRect rect = paintRect;

    auto margin = marginBox();
    rect.move(margin.left, margin.top);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);

    auto minSize = minimumSize();
    rect.setWidth(std::max<float>(rect.width(), minSize.width()));
    rect.setHeight(std::max<float>(rect.height(), minSize.height()));

    gtk_render_background(m_context.get(), cr, rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(m_context.get(), cr, rect.x(), rect.y(), rect.width(), rect.height());

    if (contentsRect) {
        auto border = borderBox();
        auto padding = paddingBox();
        *contentsRect = rect;
        contentsRect->move(border.left + padding.left, border.top + padding.top);
        contentsRect->contract(border.left + border.right + padding.left + padding.right, border.top + border.bottom + padding.top + padding.bottom);
    }

    return true;
}

void RenderThemeGadget::renderFocus(cairo_t* cr, const FloatRect& focusRect)
{
    FloatRect rect = focusRect;
    auto margin = marginBox();
    rect.move(margin.left, margin.top);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);
    gtk_render_focus(m_context.get(), cr, rect.x(), rect.y(), rect.width(), rect.height());
}

RenderThemeBoxGadget::RenderThemeBoxGadget(const RenderThemeGadget::Info& info, const Vector<RenderThemeGadget::Info> children, RenderThemeGadget* parent)
    : RenderThemeGadget(info, parent, Vector<RenderThemeGadget::Info>(), 0)
{
    m_children.reserveCapacity(children.size());
    unsigned index = 0;
    for (const auto& childInfo : children)
        m_children.uncheckedAppend(RenderThemeGadget::create(childInfo, this, children, index++));
}

IntSize RenderThemeBoxGadget::preferredSize() const
{
    IntSize minSize = RenderThemeGadget::preferredSize();
    for (const auto& child : m_children)
        minSize += child->preferredSize();
    return minSize;
}

RenderThemeTextFieldGadget::RenderThemeTextFieldGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
    : RenderThemeGadget(info, parent, siblings, position)
{
}

IntSize RenderThemeTextFieldGadget::minimumSize() const
{
    // We allow text fields smaller than the min size set on themes.
    return IntSize();
}

RenderThemeToggleGadget::RenderThemeToggleGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
    : RenderThemeGadget(info, parent, siblings, position)
    , m_type(info.type)
{
    ASSERT(m_type == RenderThemeGadget::Type::Radio || m_type == RenderThemeGadget::Type::Check);
}

bool RenderThemeToggleGadget::render(cairo_t* cr, const FloatRect& paintRect, FloatRect*)
{
    FloatRect contentsRect;
    RenderThemeGadget::render(cr, paintRect, &contentsRect);
    if (m_type == RenderThemeGadget::Type::Radio)
        gtk_render_option(m_context.get(), cr, contentsRect.x(), contentsRect.y(), contentsRect.width(), contentsRect.height());
    else
        gtk_render_check(m_context.get(), cr, contentsRect.x(), contentsRect.y(), contentsRect.width(), contentsRect.height());
    return true;
}

RenderThemeArrowGadget::RenderThemeArrowGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
    : RenderThemeGadget(info, parent, siblings, position)
{
}

bool RenderThemeArrowGadget::render(cairo_t* cr, const FloatRect& paintRect, FloatRect*)
{
    FloatRect contentsRect;
    RenderThemeGadget::render(cr, paintRect, &contentsRect);
    IntSize minSize = minimumSize();
    int arrowSize = std::min(minSize.width(), minSize.height());
    FloatPoint arrowPosition(contentsRect.x(), contentsRect.y() + (contentsRect.height() - arrowSize) / 2);
    if (gtk_style_context_get_state(m_context.get()) & GTK_STATE_FLAG_DIR_LTR)
        arrowPosition.move(contentsRect.width() - arrowSize, 0);
    gtk_render_arrow(m_context.get(), cr, G_PI / 2, arrowPosition.x(), arrowPosition.y(), arrowSize);
    return true;
}

RenderThemeIconGadget::RenderThemeIconGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
    : RenderThemeGadget(info, parent, siblings, position)
{
}

GtkIconSize RenderThemeIconGadget::gtkIconSizeForPixelSize(unsigned pixelSize) const
{
    if (pixelSize < IconSizeGtk::SmallToolbar)
        return GTK_ICON_SIZE_MENU;
    if (pixelSize >= IconSizeGtk::SmallToolbar && pixelSize < IconSizeGtk::Button)
        return GTK_ICON_SIZE_SMALL_TOOLBAR;
    if (pixelSize >= IconSizeGtk::Button && pixelSize < IconSizeGtk::LargeToolbar)
        return GTK_ICON_SIZE_BUTTON;
    if (pixelSize >= IconSizeGtk::LargeToolbar && pixelSize < IconSizeGtk::DragAndDrop)
        return GTK_ICON_SIZE_LARGE_TOOLBAR;
    if (pixelSize >= IconSizeGtk::DragAndDrop && pixelSize < IconSizeGtk::Dialog)
        return GTK_ICON_SIZE_DND;

    return GTK_ICON_SIZE_DIALOG;
}

bool RenderThemeIconGadget::render(cairo_t* cr, const FloatRect& paintRect, FloatRect*)
{
    ASSERT(!m_iconName.isNull());
    GRefPtr<GIcon> icon = adoptGRef(g_themed_icon_new(m_iconName.data()));
    unsigned lookupFlags = GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_FORCE_SVG;
    GtkTextDirection direction = gtk_style_context_get_direction(m_context.get());
    if (direction & GTK_TEXT_DIR_LTR)
        lookupFlags |= GTK_ICON_LOOKUP_DIR_LTR;
    else if (direction & GTK_TEXT_DIR_RTL)
        lookupFlags |= GTK_ICON_LOOKUP_DIR_RTL;
    int iconWidth, iconHeight;
    if (!gtk_icon_size_lookup(gtkIconSizeForPixelSize(m_iconSize), &iconWidth, &iconHeight))
        iconWidth = iconHeight = m_iconSize;
    GRefPtr<GtkIconInfo> iconInfo = adoptGRef(gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon.get(),
        std::min(iconWidth, iconHeight), static_cast<GtkIconLookupFlags>(lookupFlags)));
    if (!iconInfo)
        return false;

    GRefPtr<GdkPixbuf> iconPixbuf = adoptGRef(gtk_icon_info_load_symbolic_for_context(iconInfo.get(), m_context.get(), nullptr, nullptr));
    if (!iconPixbuf)
        return false;

    FloatSize pixbufSize(gdk_pixbuf_get_width(iconPixbuf.get()), gdk_pixbuf_get_height(iconPixbuf.get()));
    FloatRect contentsRect;
    RenderThemeGadget::render(cr, paintRect, &contentsRect);
    if (pixbufSize.width() > contentsRect.width() || pixbufSize.height() > contentsRect.height()) {
        iconWidth = iconHeight = std::min(contentsRect.width(), contentsRect.height());
        pixbufSize = FloatSize(iconWidth, iconHeight);
        iconPixbuf = adoptGRef(gdk_pixbuf_scale_simple(iconPixbuf.get(), pixbufSize.width(), pixbufSize.height(), GDK_INTERP_BILINEAR));
    }

    gtk_render_icon(m_context.get(), cr, iconPixbuf.get(), contentsRect.x() + (contentsRect.width() - pixbufSize.width()) / 2,
        contentsRect.y() + (contentsRect.height() - pixbufSize.height()) / 2);
    return true;
}

IntSize RenderThemeIconGadget::minimumSize() const
{
    if (m_iconSize < IconSizeGtk::Menu)
        return IntSize(m_iconSize, m_iconSize);

    int iconWidth, iconHeight;
    if (gtk_icon_size_lookup(gtkIconSizeForPixelSize(m_iconSize), &iconWidth, &iconHeight))
        return IntSize(iconWidth, iconHeight);

    return IntSize(m_iconSize, m_iconSize);
}

RenderThemeScrollbarGadget::RenderThemeScrollbarGadget(const RenderThemeGadget::Info& info, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position)
    : RenderThemeGadget(info, parent, siblings, position)
{
    gboolean hasBackward, hasForward, hasSecondaryBackward, hasSecondaryForward;
    gtk_style_context_get_style(m_context.get(), "has-backward-stepper", &hasBackward, "has-forward-stepper", &hasForward,
        "has-secondary-backward-stepper", &hasSecondaryBackward, "has-secondary-forward-stepper", &hasSecondaryForward, nullptr);
    if (hasBackward)
        m_steppers |= Steppers::Backward;
    if (hasForward)
        m_steppers |= Steppers::Forward;
    if (hasSecondaryBackward)
        m_steppers |= Steppers::SecondaryBackward;
    if (hasSecondaryForward)
        m_steppers |= Steppers::SecondaryForward;
}

} // namespace WebCore

#endif // GTK_CHECK_VERSION(3, 20, 0)

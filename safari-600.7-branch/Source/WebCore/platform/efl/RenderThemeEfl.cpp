/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2011 Samsung Electronics
 * Copyright (c) 2012 Intel Corporation. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderThemeEfl.h"

#include "CSSValueKeywords.h"
#include "CairoUtilitiesEfl.h"
#include "ExceptionCodePlaceholder.h"
#include "FloatRoundedRect.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "InputTypeNames.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PaintInfo.h"
#include "PlatformContextCairo.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include "ScrollbarThemeEfl.h"
#include "Settings.h"
#include "UserAgentScripts.h"
#include "UserAgentStyleSheets.h"
#include <Ecore_Evas.h>
#include <Edje.h>
#include <new>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// TODO: change from object count to ecore_evas size (bytes)
// TODO: as objects are webpage/user defined and they can be very large.
#define RENDER_THEME_EFL_PART_CACHE_MAX 32

// Initialize default font size.
float RenderThemeEfl::defaultFontSize = 16.0f;

static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;

static const float minSearchDecorationButtonSize = 1;
static const float maxSearchDecorationButtonSize = 15;
static const float searchFieldDecorationButtonOffset = 3;

// Constants for progress tag animation.
// These values have been copied from RenderThemeGtk.cpp
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;

static const int sliderThumbWidth = 29;
static const int sliderThumbHeight = 11;

#define _ASSERT_ON_RELEASE_RETURN(o, fmt, ...) \
    do { if (!o) { EINA_LOG_CRIT(fmt, ## __VA_ARGS__); ASSERT(o); return; } } while (0)
#define _ASSERT_ON_RELEASE_RETURN_VAL(o, val, fmt, ...) \
    do { if (!o) { EINA_LOG_CRIT(fmt, ## __VA_ARGS__); ASSERT(o); return val; } } while (0)


static const char* toEdjeGroup(FormType type)
{
    static const char* groups[] = {
        "webkit/widget/button",
        "webkit/widget/radio",
        "webkit/widget/entry",
        "webkit/widget/checkbox",
        "webkit/widget/combo",
        "webkit/widget/progressbar",
        "webkit/widget/search/field",
        "webkit/widget/search/results_button",
        "webkit/widget/search/results_decoration",
        "webkit/widget/search/cancel_button",
        "webkit/widget/slider/vertical",
        "webkit/widget/slider/horizontal",
        "webkit/widget/slider/thumb_vertical",
        "webkit/widget/slider/thumb_horizontal",
        "webkit/widget/spinner",
        0
    };
    ASSERT(type >= 0);
    ASSERT((size_t)type < sizeof(groups) / sizeof(groups[0])); // Out of sync?
    return groups[type];
}

static bool setSourceGroupForEdjeObject(Evas_Object* o, const String& themePath, const char* group)
{
    ASSERT(o);
    ASSERT(!themePath.isEmpty());

    if (!edje_object_file_set(o, themePath.utf8().data(), group)) {
        const char* message = edje_load_error_str(edje_object_load_error_get(o));
        EINA_LOG_ERR("Could not set theme group '%s' of file '%s': %s", group, themePath.utf8().data(), message);
        return false;
    }

    return true;
}

void RenderThemeEfl::adjustSizeConstraints(RenderStyle& style, FormType type) const
{
    loadThemeIfNeeded();

    // These are always valid, even if no theme could be loaded.
    const ThemePartDesc* desc = m_partDescs + (size_t)type;

    if (style.minWidth().isIntrinsic())
        style.setMinWidth(desc->min.width());
    if (style.minHeight().isIntrinsic())
        style.setMinHeight(desc->min.height());

    if (desc->max.width().value() > 0 && style.maxWidth().isIntrinsicOrAuto())
        style.setMaxWidth(desc->max.width());
    if (desc->max.height().value() > 0 && style.maxHeight().isIntrinsicOrAuto())
        style.setMaxHeight(desc->max.height());

    style.setPaddingTop(desc->padding.top());
    style.setPaddingBottom(desc->padding.bottom());
    style.setPaddingLeft(desc->padding.left());
    style.setPaddingRight(desc->padding.right());
}

static bool isFormElementTooLargeToDisplay(const IntSize& elementSize)
{
    // This limit of 20000 pixels is hardcoded inside edje -- anything above this size
    // will be clipped. This value seems to be reasonable enough so that hardcoding it
    // here won't be a problem.
    static const int maxEdjeDimension = 20000;

    return elementSize.width() > maxEdjeDimension || elementSize.height() > maxEdjeDimension;
}

PassOwnPtr<RenderThemeEfl::ThemePartCacheEntry> RenderThemeEfl::ThemePartCacheEntry::create(const String& themePath, FormType type, const IntSize& size)
{
    ASSERT(!themePath.isEmpty());

    if (isFormElementTooLargeToDisplay(size) || size.isEmpty()) {
        EINA_LOG_ERR("Cannot render an element of size %dx%d.", size.width(), size.height());
        return nullptr;
    }

    OwnPtr<ThemePartCacheEntry> entry = adoptPtr(new ThemePartCacheEntry);

    entry->m_canvas = EflUniquePtr<Ecore_Evas>(ecore_evas_buffer_new(size.width(), size.height()));
    if (!entry->canvas()) {
        EINA_LOG_ERR("ecore_evas_buffer_new(%d, %d) failed.", size.width(), size.height());
        return nullptr;
    }

    // By default EFL creates buffers without alpha.
    ecore_evas_alpha_set(entry->canvas(), EINA_TRUE);

    entry->m_edje = EflUniquePtr<Evas_Object>(edje_object_add(ecore_evas_get(entry->canvas())));
    ASSERT(entry->edje());

    if (!setSourceGroupForEdjeObject(entry->edje(), themePath, toEdjeGroup(type)))
        return nullptr;

    entry->m_surface = createSurfaceForBackingStore(entry->canvas());
    if (!entry->surface())
        return nullptr;

    evas_object_resize(entry->edje(), size.width(), size.height());
    evas_object_show(entry->edje());

    entry->type = type;
    entry->size = size;

    return entry.release();
}

void RenderThemeEfl::ThemePartCacheEntry::reuse(const String& themePath, FormType newType, const IntSize& newSize)
{
    ASSERT(!themePath.isEmpty());

    if (type != newType) {
        type = newType;
        if (!setSourceGroupForEdjeObject(edje(), themePath, toEdjeGroup(newType))) {
            type = FormTypeLast; // Invalidate.
            return;
        }
    }

    if (size != newSize) {
        size = newSize;
        ecore_evas_resize(canvas(), newSize.width(), newSize.height());
        evas_object_resize(edje(), newSize.width(), newSize.height());

        m_surface = createSurfaceForBackingStore(canvas());
        if (!surface()) {
            type = FormTypeLast; // Invalidate;
            return;
        }
    }
}

RenderThemeEfl::ThemePartCacheEntry* RenderThemeEfl::getThemePartFromCache(FormType type, const IntSize& size)
{
    void* data;
    Eina_List* node;
    Eina_List* reusableNode = 0;

    // Look for the item in the cache.
    EINA_LIST_FOREACH(m_partCache, node, data) {
        ThemePartCacheEntry* cachedEntry = static_cast<ThemePartCacheEntry*>(data);
        if (cachedEntry->size == size) {
            if (cachedEntry->type == type) {
                // Found the right item, move it to the head of the list
                // and return it.
                m_partCache = eina_list_promote_list(m_partCache, node);
                return cachedEntry;
            }
            // We reuse in priority the last item in the list that has
            // the requested size.
            reusableNode = node;
        }
    }

    if (eina_list_count(m_partCache) < RENDER_THEME_EFL_PART_CACHE_MAX) {
        ThemePartCacheEntry* entry = ThemePartCacheEntry::create(themePath(), type, size).leakPtr();
        if (entry)
            m_partCache = eina_list_prepend(m_partCache, entry);

        return entry;
    }

    // The cache is full, reuse the last item we found that had the
    // requested size to avoid resizing. If there was none, reuse
    // the last item of the list.
    if (!reusableNode)
        reusableNode = eina_list_last(m_partCache);

    ThemePartCacheEntry* reusedEntry = static_cast<ThemePartCacheEntry*>(eina_list_data_get(reusableNode));
    ASSERT(reusedEntry);
    reusedEntry->reuse(themePath(), type, size);
    m_partCache = eina_list_promote_list(m_partCache, reusableNode);

    return reusedEntry;
}

void RenderThemeEfl::clearThemePartCache()
{
    void* data;
    EINA_LIST_FREE(m_partCache, data)
        delete static_cast<ThemePartCacheEntry*>(data);

}

void RenderThemeEfl::applyEdjeStateFromForm(Evas_Object* object, const ControlStates* states, bool haveBackground)
{
    const char *signals[] = { // keep in sync with WebCore/platform/ThemeTypes.h
        "hovered",
        "pressed",
        "focused",
        "enabled",
        "checked",
        "read-only",
        "default",
        "window-inactive",
        "indeterminate",
        "spinup"
    };

    edje_object_signal_emit(object, "reset", "");

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(signals); ++i) {
        if (states->states() & (1 << i))
            edje_object_signal_emit(object, signals[i], "");
    }

    if (haveBackground)
        edje_object_signal_emit(object, "styled", "");
}

void RenderThemeEfl::applyEdjeRTLState(Evas_Object* edje, const RenderObject& object, FormType type, const IntRect& rect)
{
    if (type == SliderVertical || type == SliderHorizontal) {
        if (!object.isSlider())
            return; // probably have -webkit-appearance: slider..

        const RenderSlider* renderSlider = toRenderSlider(&object);
        HTMLInputElement& input = renderSlider->element();
        double valueRange = input.maximum() - input.minimum();

        OwnPtr<Edje_Message_Float_Set> msg = adoptPtr(static_cast<Edje_Message_Float_Set*>(::operator new (sizeof(Edje_Message_Float_Set) + sizeof(double))));
        msg->count = 2;

        // The first parameter of the message decides if the progress bar
        // grows from the end of the slider or from the beginning. On vertical
        // sliders, it should always be the same and will not be affected by
        // text direction settings.
        if (object.style().direction() == RTL || type == SliderVertical)
            msg->val[0] = 1;
        else
            msg->val[0] = 0;

        msg->val[1] = (input.valueAsNumber() - input.minimum()) / valueRange;
        edje_object_message_send(edje, EDJE_MESSAGE_FLOAT_SET, 0, msg.get());
    } else if (type == ProgressBar) {
        const RenderProgress& renderProgress = toRenderProgress(object);

        int max = rect.width();
        double value = renderProgress.position();

        OwnPtr<Edje_Message_Float_Set> msg = adoptPtr(static_cast<Edje_Message_Float_Set*>(::operator new (sizeof(Edje_Message_Float_Set) + sizeof(double))));
        msg->count = 2;

        if (object.style().direction() == RTL)
            msg->val[0] = (1.0 - value) * max;
        else
            msg->val[0] = 0;
        msg->val[1] = value;
        edje_object_message_send(edje, EDJE_MESSAGE_FLOAT_SET, 0, msg.get());
    }
}

bool RenderThemeEfl::isControlStyled(const RenderStyle& style, const BorderData& border, const FillLayer& background, const Color& backgroundColor) const
{
    return RenderTheme::isControlStyled(style, border, background, backgroundColor) || style.appearance() == MenulistButtonPart;
}

bool RenderThemeEfl::paintThemePart(const RenderObject& object, FormType type, const PaintInfo& info, const IntRect& rect)
{
    loadThemeIfNeeded();
    _ASSERT_ON_RELEASE_RETURN_VAL(edje(), false, "Could not paint native HTML part due to missing theme.");

    ThemePartCacheEntry* entry = getThemePartFromCache(type, rect.size());
    if (!entry)
        return false;

    bool haveBackgroundColor = isControlStyled(object.style(), object.style().border(), *object.style().backgroundLayers(), Color::white);
    ControlStates states(extractControlStatesForRenderer(object));
    applyEdjeStateFromForm(entry->edje(), &states, haveBackgroundColor);

    applyEdjeRTLState(entry->edje(), object, type, rect);

    edje_object_calc_force(entry->edje());
    edje_object_message_signal_process(entry->edje());
    evas_render(ecore_evas_get(entry->canvas()));

    cairo_t* cairo = info.context->platformContext()->cr();
    ASSERT(cairo);

    cairo_save(cairo);
    cairo_set_source_surface(cairo, entry->surface(), rect.x(), rect.y());
    cairo_paint_with_alpha(cairo, 1.0);
    cairo_restore(cairo);

    return false;
}

PassRefPtr<RenderTheme> RenderThemeEfl::create(Page* page)
{
    return adoptRef(new RenderThemeEfl(page));
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    if (page)
        return RenderThemeEfl::create(page);

    static RenderTheme* fallback = RenderThemeEfl::create(0).leakRef();
    return fallback;
}

static void applyColorCallback(void* data, Evas_Object*, const char* /* signal */, const char* colorClass)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl*>(data);
    that->setColorFromThemeClass(colorClass);
    that->platformColorsDidChange(); // Triggers relayout.
}

static bool fillColorsFromEdjeClass(Evas_Object* o, const char* colorClass, Color* color1, Color* color2 = 0, Color* color3 = 0)
{
    int r1, g1, b1, a1;
    int r2, g2, b2, a2;
    int r3, g3, b3, a3;

    if (!edje_object_color_class_get(o, colorClass, &r1, &g1, &b1, &a1, &r2, &g2, &b2, &a2, &r3, &g3, &b3, &a3))
        return false;

    if (color1)
        color1->setRGB(makeRGBA(r1, g1, b1, a1));
    if (color2)
        color2->setRGB(makeRGBA(r2, g2, b2, a2));
    if (color3)
        color3->setRGB(makeRGBA(r3, g3, b3, a3));

    return true;
}

void RenderThemeEfl::setColorFromThemeClass(const char* colorClass)
{
    ASSERT(edje());

    if (!strcmp("webkit/selection/foreground", colorClass))
        m_supportsSelectionForegroundColor = fillColorsFromEdjeClass(edje(), colorClass, &m_activeSelectionForegroundColor, &m_inactiveSelectionForegroundColor);
    else if (!strcmp("webkit/selection/background", colorClass))
        fillColorsFromEdjeClass(edje(), colorClass, &m_activeSelectionBackgroundColor, &m_inactiveSelectionBackgroundColor);
    else if (!strcmp("webkit/focus_ring", colorClass)) {
        if (!fillColorsFromEdjeClass(edje(), colorClass, &m_focusRingColor))
            return;

        // platformFocusRingColor() is only used for the default theme (without page)
        // The following is ugly, but no other way to do it unless we change it to use page themes as much as possible.
        RenderTheme::setCustomFocusRingColor(m_focusRingColor);
    }
}

void RenderThemeEfl::setThemePath(const String& newThemePath)
{
    if (newThemePath == m_themePath)
        return;

    if (newThemePath.isEmpty()) {
        EINA_LOG_CRIT("No valid theme defined, things will not work properly.");
        return;
    }

    String oldThemePath = m_themePath;
    m_themePath = newThemePath;

    // Keep the consistence by restoring the previous theme path
    // if we cannot load the new one.
    if (!loadTheme())
        m_themePath = oldThemePath;
}

String RenderThemeEfl::themePath() const
{
#ifndef NDEBUG
    if (edje()) {
        const char* path;
        edje_object_file_get(edje(), &path, 0);
        ASSERT(m_themePath == path);
    }
#endif
    return m_themePath;
}

bool RenderThemeEfl::loadTheme()
{
    ASSERT(!m_themePath.isEmpty());

    if (!canvas()) {
        m_canvas = EflUniquePtr<Ecore_Evas>(ecore_evas_buffer_new(1, 1));
        _ASSERT_ON_RELEASE_RETURN_VAL(canvas(), false,
                "Could not create canvas required by theme, things will not work properly.");
    }

    EflUniquePtr<Evas_Object> o = EflUniquePtr<Evas_Object>(edje_object_add(ecore_evas_get(canvas())));
    _ASSERT_ON_RELEASE_RETURN_VAL(o, false, "Could not create new base Edje object.");

    if (!setSourceGroupForEdjeObject(o.get(), m_themePath, "webkit/base"))
        return false; // Keep current theme.

    // Invalidate existing theme part cache.
    if (edje())
        clearThemePartCache();

    // Set new loaded theme, and apply it.
    m_edje = WTF::move(o);

    const char* thickness = edje_object_data_get(m_edje.get(), "scrollbar.thickness");
    if (thickness && !Settings::mockScrollbarsEnabled())
        static_cast<ScrollbarThemeEfl*>(ScrollbarTheme::theme())->setScrollbarThickness(atoi(thickness));

    edje_object_signal_callback_add(edje(), "color_class,set", "webkit/selection/foreground", applyColorCallback, this);
    edje_object_signal_callback_add(edje(), "color_class,set", "webkit/selection/background", applyColorCallback, this);
    edje_object_signal_callback_add(edje(), "color_class,set", "webkit/focus_ring", applyColorCallback, this);

    applyPartDescriptionsFrom(m_themePath);

    setColorFromThemeClass("webkit/selection/foreground");
    setColorFromThemeClass("webkit/selection/background");
    setColorFromThemeClass("webkit/focus_ring");

    platformColorsDidChange(); // Schedules a relayout, do last.

    return true;
}

void RenderThemeEfl::applyPartDescriptionFallback(ThemePartDesc* desc)
{
    desc->min.setWidth(Length(0, Fixed));
    desc->min.setHeight(Length(0, Fixed));

    desc->max.setWidth(Length(0, Fixed));
    desc->max.setHeight(Length(0, Fixed));

    desc->padding = LengthBox(0, 0, 0, 0);
}

void RenderThemeEfl::applyPartDescription(Evas_Object* object, ThemePartDesc* desc)
{
    Evas_Coord minw, minh, maxw, maxh;

    edje_object_size_min_get(object, &minw, &minh);
    if (!minw && !minh)
        edje_object_size_min_calc(object, &minw, &minh);

    desc->min.setWidth(Length(minw, Fixed));
    desc->min.setHeight(Length(minh, Fixed));

    edje_object_size_max_get(object, &maxw, &maxh);
    desc->max.setWidth(Length(maxw, Fixed));
    desc->max.setHeight(Length(maxh, Fixed));

    if (!edje_object_part_exists(object, "text_confinement"))
        desc->padding = LengthBox(0, 0, 0, 0);
    else {
        Evas_Coord px, py, pw, ph;
        Evas_Coord ox = 0, oy = 0, ow = 0, oh = 0;
        int t, r, b, l;

        if (minw > 0)
            ow = minw;
        else
            ow = 100;
        if (minh > 0)
            oh = minh;
        else
            oh = 100;
        if (maxw > 0 && ow > maxw)
            ow = maxw;
        if (maxh > 0 && oh > maxh)
            oh = maxh;

        evas_object_move(object, ox, oy);
        evas_object_resize(object, ow, oh);
        edje_object_calc_force(object);
        edje_object_message_signal_process(object);
        edje_object_part_geometry_get(object, "text_confinement", &px, &py, &pw, &ph);

        t = py - oy;
        b = (oh + oy) - (ph + py);

        l = px - ox;
        r = (ow + ox) - (pw + px);

        desc->padding = LengthBox(t, r, b, l);
    }
}

void RenderThemeEfl::applyPartDescriptionsFrom(const String& themePath)
{
    EflUniquePtr<Evas_Object> temp = EflUniquePtr<Evas_Object>(edje_object_add(ecore_evas_get(canvas())));
    _ASSERT_ON_RELEASE_RETURN(temp, "Could not create Edje object.");

    for (size_t i = 0; i < FormTypeLast; i++) {
        FormType type = static_cast<FormType>(i);
        m_partDescs[i].type = type;
        if (!setSourceGroupForEdjeObject(temp.get(), themePath, toEdjeGroup(type)))
            applyPartDescriptionFallback(m_partDescs + i);
        else
            applyPartDescription(temp.get(), m_partDescs + i);
    }
}

RenderThemeEfl::RenderThemeEfl(Page* page)
    : RenderTheme()
    , m_page(page)
    , m_activeSelectionBackgroundColor(0, 0, 255)
    , m_activeSelectionForegroundColor(Color::white)
    , m_inactiveSelectionBackgroundColor(0, 0, 128)
    , m_inactiveSelectionForegroundColor(200, 200, 200)
    , m_focusRingColor(32, 32, 224, 224)
    , m_sliderThumbColor(Color::darkGray)
    , m_supportsSelectionForegroundColor(false)
    , m_partCache(0)
{
}

RenderThemeEfl::~RenderThemeEfl()
{
    clearThemePartCache();
}

static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
    case TextAreaPart:
    case SearchFieldPart:
    case MenulistPart:
    case RadioPart:
    case CheckboxPart:
    case SliderVerticalPart:
    case SliderHorizontalPart:
        return true;
    default:
        return false;
    }
}

bool RenderThemeEfl::supportsFocusRing(const RenderStyle& style) const
{
    return supportsFocus(style.appearance());
}

bool RenderThemeEfl::controlSupportsTints(const RenderObject& object) const
{
    return isEnabled(object);
}

int RenderThemeEfl::baselinePosition(const RenderObject& object) const
{
    if (!object.isBox())
        return 0;

    if (object.style().appearance() == CheckboxPart
    ||  object.style().appearance() == RadioPart)
        return toRenderBox(&object)->marginTop() + toRenderBox(&object)->height() - 3;

    return RenderTheme::baselinePosition(object);
}

Color RenderThemeEfl::platformActiveSelectionBackgroundColor() const
{
    loadThemeIfNeeded();
    return m_activeSelectionBackgroundColor;
}

Color RenderThemeEfl::platformInactiveSelectionBackgroundColor() const
{
    loadThemeIfNeeded();
    return m_inactiveSelectionBackgroundColor;
}

Color RenderThemeEfl::platformActiveSelectionForegroundColor() const
{
    loadThemeIfNeeded();
    return m_activeSelectionForegroundColor;
}

Color RenderThemeEfl::platformInactiveSelectionForegroundColor() const
{
    loadThemeIfNeeded();
    return m_inactiveSelectionForegroundColor;
}

Color RenderThemeEfl::platformFocusRingColor() const
{
    loadThemeIfNeeded();
    return m_focusRingColor;
}

bool RenderThemeEfl::supportsSelectionForegroundColors() const
{
    loadThemeIfNeeded();
    return m_supportsSelectionForegroundColor;
}

bool RenderThemeEfl::paintSliderTrack(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    if (object.style().appearance() == SliderHorizontalPart)
        paintThemePart(object, SliderHorizontal, info, rect);
    else
        paintThemePart(object, SliderVertical, info, rect);

#if ENABLE(DATALIST_ELEMENT)
    paintSliderTicks(object, info, rect);
#endif

    return false;
}

void RenderThemeEfl::adjustSliderTrackStyle(StyleResolver&, RenderStyle& style, Element&) const
{
    style.setBoxShadow(nullptr);
}

void RenderThemeEfl::adjustSliderThumbStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    RenderTheme::adjustSliderThumbStyle(styleResolver, style, element);
    style.setBoxShadow(nullptr);
}

void RenderThemeEfl::adjustSliderThumbSize(RenderStyle& style, Element&) const
{
    ControlPart part = style.appearance();
    if (part == SliderThumbVerticalPart) {
        style.setWidth(Length(sliderThumbHeight, Fixed));
        style.setHeight(Length(sliderThumbWidth, Fixed));
    } else if (part == SliderThumbHorizontalPart) {
        style.setWidth(Length(sliderThumbWidth, Fixed));
        style.setHeight(Length(sliderThumbHeight, Fixed));
    }
}

#if ENABLE(DATALIST_ELEMENT)
IntSize RenderThemeEfl::sliderTickSize() const
{
    return IntSize(1, 6);
}

int RenderThemeEfl::sliderTickOffsetFromTrackCenter() const
{
    static const int sliderTickOffset = -12;

    return sliderTickOffset;
}

LayoutUnit RenderThemeEfl::sliderTickSnappingThreshold() const
{
    // The same threshold value as the Chromium port.
    return 5;
}
#endif

bool RenderThemeEfl::supportsDataListUI(const AtomicString& type) const
{
#if ENABLE(DATALIST_ELEMENT)
    // FIXME: We need to support other types.
    return type == InputTypeNames::email()
        || type == InputTypeNames::range()
        || type == InputTypeNames::search()
        || type == InputTypeNames::url();
#else
    UNUSED_PARAM(type);
    return false;
#endif
}

bool RenderThemeEfl::paintSliderThumb(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    if (object.style().appearance() == SliderThumbHorizontalPart)
        paintThemePart(object, SliderThumbHorizontal, info, rect);
    else
        paintThemePart(object, SliderThumbVertical, info, rect);

    return false;
}

void RenderThemeEfl::adjustCheckboxStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustCheckboxStyle(styleResolver, style, element);
        return;
    }

    adjustSizeConstraints(style, CheckBox);

    style.resetBorder();

    const ThemePartDesc* desc = m_partDescs + (size_t)CheckBox;
    if (style.width().value() < desc->min.width().value())
        style.setWidth(desc->min.width());
    if (style.height().value() < desc->min.height().value())
        style.setHeight(desc->min.height());
}

bool RenderThemeEfl::paintCheckbox(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, CheckBox, info, rect);
}

void RenderThemeEfl::adjustRadioStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustRadioStyle(styleResolver, style, element);
        return;
    }

    adjustSizeConstraints(style, RadioButton);

    style.resetBorder();

    const ThemePartDesc* desc = m_partDescs + (size_t)RadioButton;
    if (style.width().value() < desc->min.width().value())
        style.setWidth(desc->min.width());
    if (style.height().value() < desc->min.height().value())
        style.setHeight(desc->min.height());
}

bool RenderThemeEfl::paintRadio(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, RadioButton, info, rect);
}

void RenderThemeEfl::adjustButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustButtonStyle(styleResolver, style, element);
        return;
    }

    // adjustSizeConstrains can make SquareButtonPart's size wrong (by adjusting paddings), so call it only for PushButtonPart and ButtonPart
    if (style.appearance() == PushButtonPart || style.appearance() == ButtonPart)
        adjustSizeConstraints(style, Button);
}

bool RenderThemeEfl::paintButton(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, Button, info, rect);
}

void RenderThemeEfl::adjustMenuListStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustMenuListStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, ComboBox);
    style.resetBorder();
    style.setWhiteSpace(PRE);

    style.setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeEfl::paintMenuList(const RenderObject& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintThemePart(object, ComboBox, info, IntRect(rect));
}

void RenderThemeEfl::adjustMenuListButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    // Height is locked to auto if height is not specified.
    style.setHeight(Length(Auto));

    // The <select> box must be at least 12px high for the button to render the text inside the box without clipping.
    const int dropDownBoxMinHeight = 12;

    // Calculate min-height of the <select> element.
    int minHeight = style.fontMetrics().height();
    minHeight = std::max(minHeight, dropDownBoxMinHeight);
    style.setMinHeight(Length(minHeight, Fixed));

    adjustMenuListStyle(styleResolver, style, element);
}

bool RenderThemeEfl::paintMenuListButtonDecorations(const RenderObject& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintMenuList(object, info, rect);
}

void RenderThemeEfl::adjustTextFieldStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustTextFieldStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, TextField);
    style.resetBorder();
}

bool RenderThemeEfl::paintTextField(const RenderObject& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintThemePart(object, TextField, info, IntRect(rect));
}

void RenderThemeEfl::adjustTextAreaStyle(StyleResolver&, RenderStyle&, Element&) const
{
}

bool RenderThemeEfl::paintTextArea(const RenderObject& object, const PaintInfo& info, const FloatRect& rect)
{
    return paintTextField(object, info, rect);
}

void RenderThemeEfl::adjustSearchFieldResultsButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustSearchFieldResultsButtonStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, SearchFieldResultsButton);
    style.resetBorder();
    style.setWhiteSpace(PRE);

    float fontScale = style.fontSize() / defaultFontSize;
    int decorationSize = lroundf(std::min(std::max(minSearchDecorationButtonSize, defaultFontSize * fontScale), maxSearchDecorationButtonSize));

    style.setWidth(Length(decorationSize + searchFieldDecorationButtonOffset, Fixed));
    style.setHeight(Length(decorationSize, Fixed));
}

bool RenderThemeEfl::paintSearchFieldResultsButton(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, SearchFieldResultsButton, info, rect);
}

void RenderThemeEfl::adjustSearchFieldResultsDecorationPartStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustSearchFieldResultsDecorationPartStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, SearchFieldResultsDecoration);
    style.resetBorder();
    style.setWhiteSpace(PRE);

    float fontScale = style.fontSize() / defaultFontSize;
    int decorationSize = lroundf(std::min(std::max(minSearchDecorationButtonSize, defaultFontSize * fontScale), maxSearchDecorationButtonSize));

    style.setWidth(Length(decorationSize + searchFieldDecorationButtonOffset, Fixed));
    style.setHeight(Length(decorationSize, Fixed));
}

bool RenderThemeEfl::paintSearchFieldResultsDecorationPart(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, SearchFieldResultsDecoration, info, rect);
}

void RenderThemeEfl::adjustSearchFieldCancelButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustSearchFieldCancelButtonStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, SearchFieldCancelButton);
    style.resetBorder();
    style.setWhiteSpace(PRE);

    // Logic taken from RenderThemeChromium.cpp.
    // Scale the button size based on the font size.
    float fontScale = style.fontSize() / defaultFontSize;
    int cancelButtonSize = lroundf(std::min(std::max(minCancelButtonSize, defaultFontSize * fontScale), maxCancelButtonSize));

    style.setWidth(Length(cancelButtonSize, Fixed));
    style.setHeight(Length(cancelButtonSize, Fixed));
}

bool RenderThemeEfl::paintSearchFieldCancelButton(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, SearchFieldCancelButton, info, rect);
}

void RenderThemeEfl::adjustSearchFieldStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustSearchFieldStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, SearchField);
    style.resetBorder();
    style.setWhiteSpace(PRE);
}

bool RenderThemeEfl::paintSearchField(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, SearchField, info, rect);
}

void RenderThemeEfl::adjustInnerSpinButtonStyle(StyleResolver& styleResolver, RenderStyle& style, Element& element) const
{
    if (!m_page && element.document().page()) {
        static_cast<RenderThemeEfl&>(element.document().page()->theme()).adjustInnerSpinButtonStyle(styleResolver, style, element);
        return;
    }
    adjustSizeConstraints(style, Spinner);
}

bool RenderThemeEfl::paintInnerSpinButton(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    return paintThemePart(object, Spinner, info, rect);
}

void RenderThemeEfl::setDefaultFontSize(int size)
{
    defaultFontSize = size;
}

void RenderThemeEfl::systemFont(CSSValueID, FontDescription& fontDescription) const
{
    // It was called by RenderEmbeddedObject::paintReplaced to render alternative string.
    // To avoid cairo_error while rendering, fontDescription should be passed.
    fontDescription.setOneFamily("Sans");
    fontDescription.setSpecifiedSize(defaultFontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

void RenderThemeEfl::adjustProgressBarStyle(StyleResolver&, RenderStyle& style, Element&) const
{
    style.setBoxShadow(nullptr);
}

double RenderThemeEfl::animationRepeatIntervalForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval;
}

double RenderThemeEfl::animationDurationForProgressBar(RenderProgress&) const
{
    return progressAnimationInterval * progressAnimationFrames * 2; // "2" for back and forth;
}

bool RenderThemeEfl::paintProgressBar(const RenderObject& object, const PaintInfo& info, const IntRect& rect)
{
    if (!object.isProgress())
        return true;

    return paintThemePart(object, ProgressBar, info, rect);
}

#if ENABLE(VIDEO)
String RenderThemeEfl::mediaControlsStyleSheet()
{
    return ASCIILiteral(mediaControlsAppleUserAgentStyleSheet);
}

String RenderThemeEfl::mediaControlsScript()
{
    StringBuilder scriptBuilder;
    scriptBuilder.append(mediaControlsLocalizedStringsJavaScript, sizeof(mediaControlsLocalizedStringsJavaScript));
    scriptBuilder.append(mediaControlsAppleJavaScript, sizeof(mediaControlsAppleJavaScript));
    return scriptBuilder.toString();
}
#endif

#undef _ASSERT_ON_RELEASE_RETURN
#undef _ASSERT_ON_RELEASE_RETURN_VAL

}

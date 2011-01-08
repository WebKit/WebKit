/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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
#include "FileSystem.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include <wtf/text/CString.h>

#include <Ecore_Evas.h>
#include <Edje.h>
namespace WebCore {

// TODO: change from object count to ecore_evas size (bytes)
// TODO: as objects are webpage/user defined and they can be very large.
#define RENDER_THEME_EFL_PART_CACHE_MAX 32

void RenderThemeEfl::adjustSizeConstraints(RenderStyle* style, FormType type) const
{
    const struct ThemePartDesc* desc = m_partDescs + (size_t)type;

    if (style->minWidth().isIntrinsicOrAuto())
        style->setMinWidth(desc->min.width());
    if (style->minHeight().isIntrinsicOrAuto())
        style->setMinHeight(desc->min.height());

    if (desc->max.width().value() > 0 && style->maxWidth().isIntrinsicOrAuto())
        style->setMaxWidth(desc->max.width());
    if (desc->max.height().value() > 0 && style->maxHeight().isIntrinsicOrAuto())
        style->setMaxHeight(desc->max.height());

    style->setPaddingTop(desc->padding.top());
    style->setPaddingBottom(desc->padding.bottom());
    style->setPaddingLeft(desc->padding.left());
    style->setPaddingRight(desc->padding.right());
}

bool RenderThemeEfl::themePartCacheEntryReset(struct ThemePartCacheEntry* ce, FormType type)
{
    const char *file, *group;

    ASSERT(ce);

    edje_object_file_get(m_edje, &file, 0);
    group = edjeGroupFromFormType(type);
    ASSERT(file);
    ASSERT(group);

    if (!edje_object_file_set(ce->o, file, group)) {
        Edje_Load_Error err = edje_object_load_error_get(ce->o);
        const char *errmsg = edje_load_error_str(err);
        EINA_LOG_ERR("Could not load '%s' from theme %s: %s",
                     group, file, errmsg);
        return false;
    }
    return true;
}

bool RenderThemeEfl::themePartCacheEntrySurfaceCreate(struct ThemePartCacheEntry* ce)
{
    int w, h;
    cairo_status_t status;

    ASSERT(ce);
    ASSERT(ce->ee);

    ecore_evas_geometry_get(ce->ee, 0, 0, &w, &h);
    ASSERT(w > 0);
    ASSERT(h > 0);

    ce->surface = cairo_image_surface_create_for_data((unsigned char *)ecore_evas_buffer_pixels_get(ce->ee),
                                                      CAIRO_FORMAT_ARGB32, w, h, w * 4);
    status = cairo_surface_status(ce->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        EINA_LOG_ERR("Could not create cairo surface: %s",
                     cairo_status_to_string(status));
        return false;
    }

    return true;
}

// allocate a new entry and fill it with edje group
struct RenderThemeEfl::ThemePartCacheEntry* RenderThemeEfl::cacheThemePartNew(FormType type, const IntSize& size)
{
    struct ThemePartCacheEntry *ce = new struct ThemePartCacheEntry;

    if (!ce) {
        EINA_LOG_ERR("could not allocate ThemePartCacheEntry.");
        return 0;
    }

    ce->ee = ecore_evas_buffer_new(size.width(), size.height());
    if (!ce->ee) {
        EINA_LOG_ERR("ecore_evas_buffer_new(%d, %d) failed.",
                     size.width(), size.height());
        delete ce;
        return 0;
    }

    ce->o = edje_object_add(ecore_evas_get(ce->ee));
    ASSERT(ce->o);
    if (!themePartCacheEntryReset(ce, type)) {
        evas_object_del(ce->o);
        ecore_evas_free(ce->ee);
        delete ce;
        return 0;
    }

    if (!themePartCacheEntrySurfaceCreate(ce)) {
        evas_object_del(ce->o);
        ecore_evas_free(ce->ee);
        delete ce;
        return 0;
    }

    evas_object_resize(ce->o, size.width(), size.height());
    evas_object_show(ce->o);

    ce->type = type;
    ce->size = size;

    m_partCache.prepend(ce);
    return ce;
}

// just change the edje group and return the same entry
struct RenderThemeEfl::ThemePartCacheEntry* RenderThemeEfl::cacheThemePartReset(FormType type, struct RenderThemeEfl::ThemePartCacheEntry* ce)
{
    if (!themePartCacheEntryReset(ce, type)) {
        ce->type = FormTypeLast; // invalidate
        m_partCache.append(ce);
        return 0;
    }
    ce->type = type;
    m_partCache.prepend(ce);
    return ce;
}

// resize entry and reset it
struct RenderThemeEfl::ThemePartCacheEntry* RenderThemeEfl::cacheThemePartResizeAndReset(FormType type, const IntSize& size, struct RenderThemeEfl::ThemePartCacheEntry* ce)
{
    cairo_surface_finish(ce->surface);
    ecore_evas_resize(ce->ee, size.width(), size.height());
    evas_object_resize(ce->o, size.width(), size.height());

    if (!themePartCacheEntrySurfaceCreate(ce)) {
        evas_object_del(ce->o);
        ecore_evas_free(ce->ee);
        delete ce;
        return 0;
    }

    return cacheThemePartReset(type, ce);
}

// general purpose get (will create, reuse and all)
struct RenderThemeEfl::ThemePartCacheEntry* RenderThemeEfl::cacheThemePartGet(FormType type, const IntSize& size)
{
    Vector<struct ThemePartCacheEntry *>::iterator itr, end;
    struct ThemePartCacheEntry *ce_last_size = 0;
    int i, idxLastSize = -1;

    itr = m_partCache.begin();
    end = m_partCache.end();
    for (i = 0; itr != end; i++, itr++) {
        struct ThemePartCacheEntry *ce = *itr;
        if (ce->size == size) {
            if (ce->type == type)
                return ce;
            ce_last_size = ce;
            idxLastSize = i;
        }
    }

    if (m_partCache.size() < RENDER_THEME_EFL_PART_CACHE_MAX)
        return cacheThemePartNew(type, size);

    if (ce_last_size && ce_last_size != m_partCache.first()) {
        m_partCache.remove(idxLastSize);
        return cacheThemePartReset(type, ce_last_size);
    }

    ThemePartCacheEntry* ce = m_partCache.last();
    m_partCache.removeLast();
    return cacheThemePartResizeAndReset(type, size, ce);
}

void RenderThemeEfl::cacheThemePartFlush()
{
    Vector<struct ThemePartCacheEntry *>::iterator itr, end;

    itr = m_partCache.begin();
    end = m_partCache.end();
    for (; itr != end; itr++) {
        struct ThemePartCacheEntry *ce = *itr;
        cairo_surface_finish(ce->surface);
        evas_object_del(ce->o);
        ecore_evas_free(ce->ee);
        delete ce;
    }
    m_partCache.clear();
}

void RenderThemeEfl::applyEdjeStateFromForm(Evas_Object* o, ControlStates states)
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
        "indeterminate"
    };

    edje_object_signal_emit(o, "reset", "");

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(signals); ++i) {
        if (states & (1 << i))
            edje_object_signal_emit(o, signals[i], "");
    }
}

bool RenderThemeEfl::paintThemePart(RenderObject* o, FormType type, const PaintInfo& i, const IntRect& rect)
{
    ThemePartCacheEntry* ce;
    Eina_List* updates;
    cairo_t* cairo;

    ASSERT(m_canvas);
    ASSERT(m_edje);

    ce = cacheThemePartGet(type, rect.size());
    ASSERT(ce);
    if (!ce)
        return false;

    applyEdjeStateFromForm(ce->o, controlStatesForRenderer(o));

    cairo = i.context->platformContext();
    ASSERT(cairo);

    // Currently, only sliders needs this message; if other widget ever needs special
    // treatment, move them to special functions.
    if (type == SliderVertical || type == SliderHorizontal) {
        RenderSlider* renderSlider = toRenderSlider(o);
        Edje_Message_Float_Set* msg;
        int max, value;

        if (type == SliderVertical) {
            max = rect.height() - renderSlider->thumbRect().height();
            value = renderSlider->thumbRect().y();
        } else {
            max = rect.width() - renderSlider->thumbRect().width();
            value = renderSlider->thumbRect().x();
        }

        msg = static_cast<Edje_Message_Float_Set*>(alloca(sizeof(Edje_Message_Float_Set) + sizeof(float)));

        msg->count = 2;
        msg->val[0] = static_cast<float>(value) / static_cast<float>(max);
        msg->val[1] = 0.1;
        edje_object_message_send(ce->o, EDJE_MESSAGE_FLOAT_SET, 0, msg);
#if ENABLE(PROGRESS_TAG)
    } else if (type == ProgressBar) {
        RenderProgress* renderProgress = toRenderProgress(o);
        Edje_Message_Float_Set* msg;
        int max;
        double value;

        msg = static_cast<Edje_Message_Float_Set*>(alloca(sizeof(Edje_Message_Float_Set) + sizeof(float)));
        max = rect.width();
        value = renderProgress->position();

        msg->count = 2;
        if (o->style()->direction() == RTL)
            msg->val[0] = (1.0 - value) * max;
        else
            msg->val[0] = 0;
        msg->val[1] = value;
        edje_object_message_send(ce->o, EDJE_MESSAGE_FLOAT_SET, 0, msg);
#endif
    }

    edje_object_calc_force(ce->o);
    edje_object_message_signal_process(ce->o);
    updates = evas_render_updates(ecore_evas_get(ce->ee));
    evas_render_updates_free(updates);

    cairo_save(cairo);
    cairo_set_source_surface(cairo, ce->surface, rect.x(), rect.y());
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

    static RenderTheme* fallback = RenderThemeEfl::create(0).releaseRef();
    return fallback;
}

static void renderThemeEflColorClassSelectionActive(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setActiveSelectionColor(fr, fg, fb, fa, br, bg, bb, ba);
}

static void renderThemeEflColorClassSelectionInactive(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setInactiveSelectionColor(fr, fg, fb, fa, br, bg, bb, ba);
}

static void renderThemeEflColorClassFocusRing(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, 0, 0, 0, 0, 0, 0, 0, 0))
        return;

    that->setFocusRingColor(fr, fg, fb, fa);
}

static void renderThemeEflColorClassButtonText(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setButtonTextColor(fr, fg, fb, fa, br, bg, bb, ba);
}

static void renderThemeEflColorClassComboText(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setComboTextColor(fr, fg, fb, fa, br, bg, bb, ba);
}

static void renderThemeEflColorClassEntryText(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;

    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setEntryTextColor(fr, fg, fb, fa, br, bg, bb, ba);
}

static void renderThemeEflColorClassSearchText(void* data, Evas_Object* o, const char* signal, const char* source)
{
    RenderThemeEfl* that = static_cast<RenderThemeEfl *>(data);
    int fr, fg, fb, fa, br, bg, bb, ba;
    if (!edje_object_color_class_get(o, source, &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, 0, 0, 0, 0))
        return;

    that->setSearchTextColor(fr, fg, fb, fa, br, bg, bb, ba);
}

void RenderThemeEfl::createCanvas()
{
    ASSERT(!m_canvas);
    m_canvas = ecore_evas_buffer_new(1, 1);
    ASSERT(m_canvas);
}

void RenderThemeEfl::createEdje()
{
    ASSERT(!m_edje);
    Frame* frame = m_page ? m_page->mainFrame() : 0;
    FrameView* view = frame ? frame->view() : 0;
    String theme = view ? view->edjeThemeRecursive() : "";
    if (theme.isEmpty())
        EINA_LOG_ERR("No theme defined, unable to set RenderThemeEfl.");
    else {
        m_edje = edje_object_add(ecore_evas_get(m_canvas));
        if (!m_edje)
            EINA_LOG_ERR("Could not create base edje object.");
        else if (!edje_object_file_set(m_edje, theme.utf8().data(), "webkit/base")) {
            Edje_Load_Error err = edje_object_load_error_get(m_edje);
            const char* errmsg = edje_load_error_str(err);
            EINA_LOG_ERR("Could not load 'webkit/base' from theme %s: %s",
                         theme.utf8().data(), errmsg);
            evas_object_del(m_edje);
            m_edje = 0;
        } else {
#define CONNECT(cc, func)                                               \
            edje_object_signal_callback_add(m_edje, "color_class,set",  \
                                            "webkit/"cc, func, this)

            CONNECT("selection/active",
                    renderThemeEflColorClassSelectionActive);
            CONNECT("selection/inactive",
                    renderThemeEflColorClassSelectionInactive);
            CONNECT("focus_ring", renderThemeEflColorClassFocusRing);
            CONNECT("button/text", renderThemeEflColorClassButtonText);
            CONNECT("combo/text", renderThemeEflColorClassComboText);
            CONNECT("entry/text", renderThemeEflColorClassEntryText);
            CONNECT("search/text", renderThemeEflColorClassSearchText);
#undef CONNECT
        }
    }
    ASSERT(m_edje);
}

void RenderThemeEfl::applyEdjeColors()
{
    int fr, fg, fb, fa, br, bg, bb, ba;
    ASSERT(m_edje);
#define COLOR_GET(cls)                                                  \
    edje_object_color_class_get(m_edje, "webkit/"cls,                   \
                                &fr, &fg, &fb, &fa, &br, &bg, &bb, &ba, \
                                0, 0, 0, 0)

    if (COLOR_GET("selection/active")) {
        m_activeSelectionForegroundColor = Color(fr, fg, fb, fa);
        m_activeSelectionBackgroundColor = Color(br, bg, bb, ba);
    }
    if (COLOR_GET("selection/inactive")) {
        m_inactiveSelectionForegroundColor = Color(fr, fg, fb, fa);
        m_inactiveSelectionBackgroundColor = Color(br, bg, bb, ba);
    }
    if (COLOR_GET("focus_ring")) {
        m_focusRingColor = Color(fr, fg, fb, fa);
        // webkit just use platformFocusRingColor() for default theme (without page)
        // this is ugly, but no other way to do it unless we change
        // it to use page themes as much as possible.
        RenderTheme::setCustomFocusRingColor(m_focusRingColor);
    }
    if (COLOR_GET("button/text")) {
        m_buttonTextForegroundColor = Color(fr, fg, fb, fa);
        m_buttonTextBackgroundColor = Color(br, bg, bb, ba);
    }
    if (COLOR_GET("combo/text")) {
        m_comboTextForegroundColor = Color(fr, fg, fb, fa);
        m_comboTextBackgroundColor = Color(br, bg, bb, ba);
    }
    if (COLOR_GET("entry/text")) {
        m_entryTextForegroundColor = Color(fr, fg, fb, fa);
        m_entryTextBackgroundColor = Color(br, bg, bb, ba);
    }
    if (COLOR_GET("search/text")) {
        m_searchTextForegroundColor = Color(fr, fg, fb, fa);
        m_searchTextBackgroundColor = Color(br, bg, bb, ba);
    }
#undef COLOR_GET
    platformColorsDidChange();
}

void RenderThemeEfl::applyPartDescriptionFallback(struct ThemePartDesc* desc)
{
    desc->min.setWidth(Length(0, Fixed));
    desc->min.setHeight(Length(0, Fixed));

    desc->max.setWidth(Length(0, Fixed));
    desc->max.setHeight(Length(0, Fixed));

    desc->padding = LengthBox(0, 0, 0, 0);
}

void RenderThemeEfl::applyPartDescription(Evas_Object* o, struct ThemePartDesc* desc)
{
    Evas_Coord minw, minh, maxw, maxh;

    edje_object_size_min_get(o, &minw, &minh);
    if (!minw && !minh)
        edje_object_size_min_calc(o, &minw, &minh);

    desc->min.setWidth(Length(minw, Fixed));
    desc->min.setHeight(Length(minh, Fixed));

    edje_object_size_max_get(o, &maxw, &maxh);
    desc->max.setWidth(Length(maxw, Fixed));
    desc->max.setHeight(Length(maxh, Fixed));

    if (!edje_object_part_exists(o, "text_confinement"))
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

        evas_object_move(o, ox, oy);
        evas_object_resize(o, ow, oh);
        edje_object_calc_force(o);
        edje_object_message_signal_process(o);
        edje_object_part_geometry_get(o, "text_confinement", &px, &py, &pw, &ph);

        t = py - oy;
        b = (oh + oy) - (ph + py);

        l = px - ox;
        r = (ow + ox) - (pw + px);

        desc->padding = LengthBox(t, r, b, l);
    }
}

const char* RenderThemeEfl::edjeGroupFromFormType(FormType type) const
{
    static const char* groups[] = {
#define W(n) "webkit/widget/"n
        W("button"),
        W("radio"),
        W("entry"),
        W("checkbox"),
        W("combo"),
#if ENABLE(PROGRESS_TAG)
        W("progressbar"),
#endif
        W("search/field"),
        W("search/decoration"),
        W("search/results_button"),
        W("search/results_decoration"),
        W("search/cancel_button"),
        W("slider/vertical"),
        W("slider/horizontal"),
#undef W
        0
    };
    ASSERT(type >= 0);
    ASSERT((size_t)type < sizeof(groups) / sizeof(groups[0])); // out of sync?
    return groups[type];
}

void RenderThemeEfl::applyPartDescriptions()
{
    Evas_Object* o;
    unsigned int i;
    const char* file;

    ASSERT(m_canvas);
    ASSERT(m_edje);

    edje_object_file_get(m_edje, &file, 0);
    ASSERT(file);

    o = edje_object_add(ecore_evas_get(m_canvas));
    if (!o) {
        EINA_LOG_ERR("Could not create Edje object.");
        return;
    }

    for (i = 0; i < FormTypeLast; i++) {
        FormType type = static_cast<FormType>(i);
        const char* group = edjeGroupFromFormType(type);
        m_partDescs[i].type = type;
        if (!edje_object_file_set(o, file, group)) {
            Edje_Load_Error err = edje_object_load_error_get(o);
            const char* errmsg = edje_load_error_str(err);
            EINA_LOG_ERR("Could not set theme group '%s' of file '%s': %s",
                         group, file, errmsg);

            applyPartDescriptionFallback(m_partDescs + i);
        } else
            applyPartDescription(o, m_partDescs + i);
    }
    evas_object_del(o);
}

void RenderThemeEfl::themeChanged()
{
    cacheThemePartFlush();

    if (!m_canvas) {
        createCanvas();
        if (!m_canvas)
            return;
    }

    if (!m_edje) {
        createEdje();
        if (!m_edje)
            return;
    }

    applyEdjeColors();
    applyPartDescriptions();
}

float RenderThemeEfl::defaultFontSize = 16.0f;

RenderThemeEfl::RenderThemeEfl(Page* page)
    : RenderTheme()
    , m_page(page)
    , m_activeSelectionBackgroundColor(0, 0, 255)
    , m_activeSelectionForegroundColor(255, 255, 255)
    , m_inactiveSelectionBackgroundColor(0, 0, 128)
    , m_inactiveSelectionForegroundColor(200, 200, 200)
    , m_focusRingColor(32, 32, 224, 224)
    , m_buttonTextBackgroundColor(0, 0, 0, 0)
    , m_buttonTextForegroundColor(0, 0, 0)
    , m_comboTextBackgroundColor(0, 0, 0, 0)
    , m_comboTextForegroundColor(0, 0, 0)
    , m_entryTextBackgroundColor(0, 0, 0, 0)
    , m_entryTextForegroundColor(0, 0, 0)
    , m_searchTextBackgroundColor(0, 0, 0, 0)
    , m_searchTextForegroundColor(0, 0, 0)
    , m_canvas(0)
    , m_edje(0)
{
    if (page && page->mainFrame() && page->mainFrame()->view())
        themeChanged();
}

RenderThemeEfl::~RenderThemeEfl()
{
    cacheThemePartFlush();

    if (m_canvas) {
        if (m_edje)
            evas_object_del(m_edje);
        ecore_evas_free(m_canvas);
    }
}

void RenderThemeEfl::setActiveSelectionColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_activeSelectionForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_activeSelectionBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
}

void RenderThemeEfl::setInactiveSelectionColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_inactiveSelectionForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_inactiveSelectionBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
}

void RenderThemeEfl::setFocusRingColor(int r, int g, int b, int a)
{
    m_focusRingColor = Color(r, g, b, a);
    // webkit just use platformFocusRingColor() for default theme (without page)
    // this is ugly, but no other way to do it unless we change
    // it to use page themes as much as possible.
    RenderTheme::setCustomFocusRingColor(m_focusRingColor);
    platformColorsDidChange();
}

void RenderThemeEfl::setButtonTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_buttonTextForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_buttonTextBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
}

void RenderThemeEfl::setComboTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_comboTextForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_comboTextBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
}

void RenderThemeEfl::setEntryTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_entryTextForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_entryTextBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
}

void RenderThemeEfl::setSearchTextColor(int foreR, int foreG, int foreB, int foreA, int backR, int backG, int backB, int backA)
{
    m_searchTextForegroundColor = Color(foreR, foreG, foreB, foreA);
    m_searchTextBackgroundColor = Color(backR, backG, backB, backA);
    platformColorsDidChange();
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

bool RenderThemeEfl::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

bool RenderThemeEfl::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

int RenderThemeEfl::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    if (o->style()->appearance() == CheckboxPart
    ||  o->style()->appearance() == RadioPart)
        return toRenderBox(o)->marginTop() + toRenderBox(o)->height() - 3;

    return RenderTheme::baselinePosition(o);
}

bool RenderThemeEfl::paintSliderTrack(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    if (o->style()->appearance() == SliderHorizontalPart)
        return paintThemePart(o, SliderHorizontal, i, rect);
    return paintThemePart(o, SliderVertical, i, rect);
}

void RenderThemeEfl::adjustSliderTrackStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSliderTrackStyle(selector, style, e);
        return;
    }

    adjustSizeConstraints(style, SliderHorizontal);
    style->resetBorder();

    const struct ThemePartDesc *desc = m_partDescs + (size_t)SliderHorizontal;
    if (style->width().value() < desc->min.width().value())
        style->setWidth(desc->min.width());
    if (style->height().value() < desc->min.height().value())
        style->setHeight(desc->min.height());
}

void RenderThemeEfl::adjustSliderThumbStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSliderTrackStyle(selector, style, e);
}

bool RenderThemeEfl::paintSliderThumb(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintSliderTrack(o, i, rect);
}

void RenderThemeEfl::adjustCheckboxStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustCheckboxStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, CheckBox);
    style->resetBorder();

    const struct ThemePartDesc *desc = m_partDescs + (size_t)CheckBox;
    if (style->width().value() < desc->min.width().value())
        style->setWidth(desc->min.width());
    if (style->height().value() < desc->min.height().value())
        style->setHeight(desc->min.height());
}

bool RenderThemeEfl::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, CheckBox, i, rect);
}

void RenderThemeEfl::adjustRadioStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustRadioStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, RadioButton);
    style->resetBorder();

    const struct ThemePartDesc *desc = m_partDescs + (size_t)RadioButton;
    if (style->width().value() < desc->min.width().value())
        style->setWidth(desc->min.width());
    if (style->height().value() < desc->min.height().value())
        style->setHeight(desc->min.height());
}

bool RenderThemeEfl::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, RadioButton, i, rect);
}

void RenderThemeEfl::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustButtonStyle(selector, style, e);
        return;
    }

    adjustSizeConstraints(style, Button);

    if (style->appearance() == PushButtonPart) {
        style->resetBorder();
        style->setWhiteSpace(PRE);
        style->setHeight(Length(Auto));
        style->setColor(m_buttonTextForegroundColor);
        style->setBackgroundColor(m_buttonTextBackgroundColor);
    }
}

bool RenderThemeEfl::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, Button, i, rect);
}

void RenderThemeEfl::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustMenuListStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, ComboBox);
    style->resetBorder();
    style->setWhiteSpace(PRE);
    style->setColor(m_comboTextForegroundColor);
    style->setBackgroundColor(m_comboTextBackgroundColor);
}

bool RenderThemeEfl::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, ComboBox, i, rect);
}

void RenderThemeEfl::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustTextFieldStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, TextField);
    style->resetBorder();
    style->setWhiteSpace(PRE);
    style->setColor(m_entryTextForegroundColor);
    style->setBackgroundColor(m_entryTextBackgroundColor);
}

bool RenderThemeEfl::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, TextField, i, rect);
}

void RenderThemeEfl::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeEfl::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeEfl::adjustSearchFieldDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSearchFieldDecorationStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, SearchFieldDecoration);
    style->resetBorder();
    style->setWhiteSpace(PRE);
}

bool RenderThemeEfl::paintSearchFieldDecoration(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, SearchFieldDecoration, i, rect);
}

void RenderThemeEfl::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSearchFieldResultsButtonStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, SearchFieldResultsButton);
    style->resetBorder();
    style->setWhiteSpace(PRE);
}

bool RenderThemeEfl::paintSearchFieldResultsButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, SearchFieldResultsButton, i, rect);
}

void RenderThemeEfl::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSearchFieldResultsDecorationStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, SearchFieldResultsDecoration);
    style->resetBorder();
    style->setWhiteSpace(PRE);
}

bool RenderThemeEfl::paintSearchFieldResultsDecoration(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, SearchFieldResultsDecoration, i, rect);
}

void RenderThemeEfl::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSearchFieldCancelButtonStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, SearchFieldCancelButton);
    style->resetBorder();
    style->setWhiteSpace(PRE);
}

bool RenderThemeEfl::paintSearchFieldCancelButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, SearchFieldCancelButton, i, rect);
}

void RenderThemeEfl::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    if (!m_page && e && e->document()->page()) {
        static_cast<RenderThemeEfl*>(e->document()->page()->theme())->adjustSearchFieldStyle(selector, style, e);
        return;
    }
    adjustSizeConstraints(style, SearchField);
    style->resetBorder();
    style->setWhiteSpace(PRE);
    style->setColor(m_searchTextForegroundColor);
    style->setBackgroundColor(m_searchTextBackgroundColor);
}

bool RenderThemeEfl::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, SearchField, i, rect);
}

void RenderThemeEfl::setDefaultFontSize(int size)
{
    defaultFontSize = size;
}

void RenderThemeEfl::systemFont(int propId, FontDescription& fontDescription) const
{
    // It was called by RenderEmbeddedObject::paintReplaced to render alternative string.
    // To avoid cairo_error while rendering, fontDescription should be passed.
    DEFINE_STATIC_LOCAL(String, fontFace, ("Sans"));
    float fontSize = defaultFontSize;

    fontDescription.firstFamily().setFamily(fontFace);
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

#if ENABLE(PROGRESS_TAG)
void RenderThemeEfl::adjustProgressBarStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeEfl::paintProgressBar(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintThemePart(o, ProgressBar, i, rect);
}
#endif

}

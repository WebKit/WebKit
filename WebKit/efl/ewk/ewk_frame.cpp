/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

// Uncomment to view frame regions and debug messages
// #define EWK_FRAME_DEBUG

#include "config.h"
#include "ewk_frame.h"

#include "EWebKit.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameLoaderClientEfl.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLPlugInElement.h"
#include "HistoryItem.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "RefPtr.h"
#include "RenderTheme.h"
#include "ResourceRequest.h"
#include "ScriptValue.h"
#include "SharedBuffer.h"
#include "SubstituteData.h"
#include "WindowsKeyboardCodes.h"
#include "ZoomMode.h"
#include "ewk_private.h"

#include <Eina.h>
#include <Evas.h>
#include <algorithm>
#include <eina_safety_checks.h>
#include <wtf/text/CString.h>

static const char EWK_FRAME_TYPE_STR[] = "EWK_Frame";

struct Ewk_Frame_Smart_Data {
    Evas_Object_Smart_Clipped_Data base;
    Evas_Object* self;
    Evas_Object* view;
#ifdef EWK_FRAME_DEBUG
    Evas_Object* region;
#endif
    WebCore::Frame* frame;
    const char* theme;
    const char* title;
    const char* uri;
    const char* name;
    struct {
        Evas_Coord w, h;
    } contents_size;
    WebCore::ZoomMode zoom_mode;
    Eina_Bool editable:1;
};

struct Eina_Iterator_Ewk_Frame {
    Eina_Iterator base;
    Evas_Object* obj;
    WebCore::Frame* last;
};

#ifndef EWK_TYPE_CHECK
#define EWK_FRAME_TYPE_CHECK(o, ...) do { } while (0)
#else
#define EWK_FRAME_TYPE_CHECK(o, ...)                                    \
    do {                                                                \
        const char* _tmp_otype = evas_object_type_get(o);               \
        if (EINA_UNLIKELY(_tmp_otype != EWK_FRAME_TYPE_STR)) {          \
            EINA_LOG_CRIT                                               \
                ("%p (%s) is not of an ewk_frame!", o,                  \
                 _tmp_otype ? _tmp_otype : "(null)");                   \
            return __VA_ARGS__;                                         \
        }                                                               \
    } while (0)
#endif

#define EWK_FRAME_SD_GET(o, ptr)                                \
    Ewk_Frame_Smart_Data* ptr = (Ewk_Frame_Smart_Data*)evas_object_smart_data_get(o)

#define EWK_FRAME_SD_GET_OR_RETURN(o, ptr, ...)         \
    EWK_FRAME_TYPE_CHECK(o, __VA_ARGS__);               \
    EWK_FRAME_SD_GET(o, ptr);                           \
    if (!ptr) {                                         \
        CRITICAL("no smart data for object %p (%s)",    \
                 o, evas_object_type_get(o));           \
        return __VA_ARGS__;                             \
    }

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;

#ifdef EWK_FRAME_DEBUG
static inline void _ewk_frame_debug(Evas_Object* o)
{
    Evas_Object* clip, *parent;
    Evas_Coord x, y, w, h, cx, cy, cw, ch;
    int r, g, b, a, cr, cg, cb, ca;

    evas_object_color_get(o, &r, &g, &b, &a);
    evas_object_geometry_get(o, &x, &y, &w, &h);

    clip = evas_object_clip_get(o);
    evas_object_color_get(clip, &cr, &cg, &cb, &ca);
    evas_object_geometry_get(clip, &cx, &cy, &cw, &ch);

    fprintf(stderr, "%p: type=%s name=%s, visible=%d, color=%02x%02x%02x%02x, %d,%d+%dx%d, clipper=%p (%d, %02x%02x%02x%02x, %d,%d+%dx%d)\n",
            o, evas_object_type_get(o), evas_object_name_get(o), evas_object_visible_get(o),
            r, g, b, a, x, y, w, h,
            clip, evas_object_visible_get(clip), cr, cg, cb, ca, cx, cy, cw, ch);
    parent = evas_object_smart_parent_get(o);
    if (!parent)
        fprintf(stderr, "\n");
    else
        _ewk_frame_debug(parent);
}
#endif

static WebCore::FrameLoaderClientEfl* _ewk_frame_loader_efl_get(WebCore::Frame* frame)
{
    return static_cast<WebCore::FrameLoaderClientEfl*>(frame->loader()->client());
}

static inline Evas_Object* kit(WebCore::Frame* frame)
{
    if (!frame)
        return 0;
    WebCore::FrameLoaderClientEfl* fl = _ewk_frame_loader_efl_get(frame);
    if (!fl)
        return 0;
    return fl->webFrame();
}

static Eina_Bool _ewk_frame_children_iterator_next(Eina_Iterator_Ewk_Frame* it, Evas_Object** data)
{
    EWK_FRAME_SD_GET_OR_RETURN(it->obj, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);

    WebCore::FrameTree* tree = sd->frame->tree(); // check if it's still valid
    EINA_SAFETY_ON_NULL_RETURN_VAL(tree, EINA_FALSE);

    WebCore::Frame* frame;
    if (it->last)
        frame = it->last->tree()->nextSibling();
    else
        frame = tree->firstChild();

    if (!frame)
        return EINA_FALSE;

    *data = kit(frame);
    return EINA_TRUE;
}

static Evas_Object* _ewk_frame_children_iterator_get_container(Eina_Iterator_Ewk_Frame* it)
{
    return it->obj;
}

static void _ewk_frame_smart_add(Evas_Object* o)
{
    EWK_FRAME_SD_GET(o, sd);

    if (!sd) {
        sd = (Ewk_Frame_Smart_Data*)calloc(1, sizeof(Ewk_Frame_Smart_Data));
        if (!sd)
            CRITICAL("could not allocate Ewk_Frame_Smart_Data");
        else
            evas_object_smart_data_set(o, sd);
    }

    sd->self = o;

    _parent_sc.add(o);
    evas_object_move(sd->base.clipper, 0, 0);
    evas_object_resize(sd->base.clipper, 0, 0);

#ifdef EWK_FRAME_DEBUG
    sd->region = evas_object_rectangle_add(sd->base.evas);
    static int i = 0;
    switch (i) {
    case 0:
        evas_object_color_set(sd->region, 128, 0, 0, 128);
        break;
    case 1:
        evas_object_color_set(sd->region, 0, 128, 0, 128);
        break;
    case 2:
        evas_object_color_set(sd->region, 0, 0, 128, 128);
        break;
    case 3:
        evas_object_color_set(sd->region, 128, 0, 0, 128);
        break;
    case 4:
        evas_object_color_set(sd->region, 128, 128, 0, 128);
        break;
    case 5:
        evas_object_color_set(sd->region, 128, 0, 128, 128);
        break;
    case 6:
        evas_object_color_set(sd->region, 0, 128, 128, 128);
        break;
    default:
        break;
    }
    i++;
    if (i > 6)
        i = 0;

    evas_object_smart_member_add(sd->region, o);
    evas_object_hide(sd->region);
#endif
}

static void _ewk_frame_smart_del(Evas_Object* o)
{
    WRN("o=%p", o); // XXX REMOVE ME LATER
    EWK_FRAME_SD_GET(o, sd);

    if (sd) {
        if (sd->frame) {
            WebCore::FrameLoaderClientEfl* flc = _ewk_frame_loader_efl_get(sd->frame);
            flc->setWebFrame(0);
            sd->frame->loader()->cancelAndClear();
            sd->frame = 0;
        }

        eina_stringshare_del(sd->title);
        eina_stringshare_del(sd->uri);
        eina_stringshare_del(sd->name);
    }

    _parent_sc.del(o);
}

static void _ewk_frame_smart_resize(Evas_Object* o, Evas_Coord w, Evas_Coord h)
{
    EWK_FRAME_SD_GET(o, sd);
    evas_object_resize(sd->base.clipper, w, h);

#ifdef EWK_FRAME_DEBUG
    evas_object_resize(sd->region, w, h);
    Evas_Coord x, y;
    evas_object_geometry_get(sd->region, &x, &y, &w, &h);
    INF("region=%p, visible=%d, geo=%d,%d + %dx%d",
        sd->region, evas_object_visible_get(sd->region), x, y, w, h);
    _ewk_frame_debug(o);
#endif
}

static void _ewk_frame_smart_set(Evas_Smart_Class* api)
{
    evas_object_smart_clipped_smart_set(api);
    api->add = _ewk_frame_smart_add;
    api->del = _ewk_frame_smart_del;
    api->resize = _ewk_frame_smart_resize;
}

static inline Evas_Smart* _ewk_frame_smart_class_new(void)
{
    static Evas_Smart_Class sc = EVAS_SMART_CLASS_INIT_NAME_VERSION(EWK_FRAME_TYPE_STR);
    static Evas_Smart* smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        evas_object_smart_clipped_smart_set(&_parent_sc);
        _ewk_frame_smart_set(&sc);
        smart = evas_smart_class_new(&sc);
    }

    return smart;
}

/**
 * @internal
 *
 * Creates a new EFL WebKit Frame object.
 *
 * Frames are low level entries contained in a page that is contained
 * by a view. Usually one operates on the view and not directly on the
 * frame.
 *
 * @param e canvas where to create the frame object.
 *
 * @return frame object or @c NULL if errors.
 */
Evas_Object* ewk_frame_add(Evas* e)
{
    return evas_object_smart_add(e, _ewk_frame_smart_class_new());
}

/**
 * Retrieves the ewk_view object that owns this frame.
 *
 * @param o frame object to get view from.
 *
 * @return view object or @c NULL if errors.
 */
Evas_Object* ewk_frame_view_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    return sd->view;
}

/**
 * Set the theme path to be used by this frame.
 *
 * Frames inherit theme from their parent, this will have all frames
 * with unset theme to use this one.
 *
 * @param o frame object to change theme.
 * @param path theme path, may be @c NULL to reset to default or inherit parent.
 */
void ewk_frame_theme_set(Evas_Object* o, const char* path)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    if (!eina_stringshare_replace(&sd->theme, path))
        return;
    if (sd->frame && sd->frame->view()) {
        sd->frame->view()->setEdjeTheme(WTF::String(path));
        sd->frame->page()->theme()->themeChanged();
    }
}

/**
 * Gets the immediate theme set on this frame.
 *
 * This returns the value set by ewk_frame_theme_set(). Note that if
 * it is @c NULL, the frame will inherit parent's theme.
 *
 * @param o frame object to get theme path.
 *
 * @return theme path, may be @c NULL if not set.
 */
const char* ewk_frame_theme_get(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    return sd->theme;
}

/**
 * Returns a new iterator over all direct children frames.
 *
 * Keep frame object intact while iteration happens otherwise frame
 * may be destroyed while iterated.
 *
 * Iteration results are Evas_Object*, so give eina_iterator_next() a
 * pointer to it.
 *
 * @return a newly allocated iterator, free using
 *         eina_iterator_free(). If not possible to create the
 *         iterator, @c NULL is returned.
 */
Eina_Iterator* ewk_frame_children_iterator_new(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    Eina_Iterator_Ewk_Frame* it = (Eina_Iterator_Ewk_Frame*)
        calloc(1, sizeof(Eina_Iterator_Ewk_Frame));
    if (!it)
        return 0;

    EINA_MAGIC_SET(&it->base, EINA_MAGIC_ITERATOR);
    it->base.next = FUNC_ITERATOR_NEXT(_ewk_frame_children_iterator_next);
    it->base.get_container = FUNC_ITERATOR_GET_CONTAINER(_ewk_frame_children_iterator_get_container);
    it->base.free = FUNC_ITERATOR_FREE(free);
    it->obj = o;
    return &it->base;
}

/**
 * Finds a child frame by its name, recursively.
 *
 * For pre-defined names, returns @a o if @a name is "_self" or
 * "_current", returns @a o's parent frame if @a name is "_parent",
 * and returns the main frame if @a name is "_top". Also returns @a o
 * if it is the main frame and @a name is either "_parent" or
 * "_top". For other names, this function returns the first frame that
 * matches @a name. This function searches @a o and its descendents
 * first, then @a o's parent and its children moving up the hierarchy
 * until a match is found. If no match is found in @a o's hierarchy,
 * this function will search for a matching frame in other main frame
 * hierarchies.
 *
 * @return object if found, @c NULL if nothing with that name.
 */
Evas_Object* ewk_frame_child_find(Evas_Object* o, const char* name)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(name, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, 0);
    WTF::String s = WTF::String::fromUTF8(name);
    return kit(sd->frame->tree()->find(WTF::AtomicString(s)));
}

/**
 * Ask main frame to load the given URI.
 *
 * @param o frame object to load uri.
 * @param uri uniform resource identifier to load.
 */
Eina_Bool ewk_frame_uri_set(Evas_Object* o, const char* uri)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    WebCore::KURL kurl(WebCore::KURL(), WTF::String::fromUTF8(uri));
    WebCore::ResourceRequest req(kurl);
    WebCore::FrameLoader* loader = sd->frame->loader();
    loader->load(req, false);
    return EINA_TRUE;
}

/**
 * Gets the uri of this frame.
 *
 * @param o frame object to get uri.
 *
 * @return frame uri or @c NULL. It's a internal string and should
 *         not be modified. The string is guaranteed to be stringshared.
 */
const char* ewk_frame_uri_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    return sd->uri;
}

/**
 * Gets the title of this frame.
 *
 * @param o frame object to get title.
 *
 * @return frame title or @c NULL. It's a internal string and should
 *         not be modified. The string is guaranteed to be stringshared.
 */
const char* ewk_frame_title_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    return sd->title;
}

/**
 * Gets the name of this frame.
 *
 * @param o frame object to get name.
 *
 * @return frame name or @c NULL. It's a internal string and should
 *         not be modified. The string is guaranteed to be stringshared.
 */
const char* ewk_frame_name_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);

    if (sd->name)
        return sd->name;

    if (!sd->frame) {
        ERR("could not get name of uninitialized frame.");
        return 0;
    }

    WTF::String s = sd->frame->tree()->name();
    WTF::CString cs = s.utf8();
    sd->name = eina_stringshare_add_length(cs.data(), cs.length());
    return sd->name;
}

/**
 * Get last known contents size.
 *
 * @param o frame object to get contents size.
 * @param w where to store contents size width. May be @c NULL.
 * @param h where to store contents size height. May be @c NULL.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure and
 *         @a w and @a h will be zeroed.
 */
Eina_Bool ewk_frame_contents_size_get(const Evas_Object* o, Evas_Coord* w, Evas_Coord* h)
{
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    if (w)
        *w = sd->contents_size.w;
    if (h)
        *h = sd->contents_size.h;
    return EINA_TRUE;
}

static Eina_Bool _ewk_frame_contents_set_internal(Ewk_Frame_Smart_Data *sd, const char* contents, size_t contents_size, const char* mime_type, const char* encoding, const char* base_uri, const char* unreachable_uri)
{
    if (contents_size < 1)
        contents_size = strlen(contents);
    if (!mime_type)
        mime_type = "text/html";
    if (!encoding)
        encoding = "UTF-8";
    if (!base_uri)
        base_uri = "about:blank";

    WebCore::KURL baseKURL(WebCore::KURL(), WTF::String::fromUTF8(base_uri));
    WebCore::KURL unreachableKURL;
    if (unreachable_uri)
        unreachableKURL = WebCore::KURL(WebCore::KURL(), WTF::String::fromUTF8(unreachable_uri));
    else
        unreachableKURL = WebCore::KURL();

    WTF::RefPtr<WebCore::SharedBuffer> buffer = WebCore::SharedBuffer::create(contents, contents_size);
    WebCore::SubstituteData substituteData
        (buffer.release(),
         WTF::String::fromUTF8(mime_type),
         WTF::String::fromUTF8(encoding),
         baseKURL, unreachableKURL);
    WebCore::ResourceRequest request(baseKURL);

    sd->frame->loader()->load(request, substituteData, false);
    return EINA_TRUE;
}

/**
 * Requests loading the given contents in this frame.
 *
 * @param o frame object to load document.
 * @param contents what to load into frame. Must not be @c NULL.
 * @param contents_size byte size of data in @a contents.
 *        If zero, strlen() is used.
 * @param mime_type type of @a contents data. If @c NULL "text/html" is assumed.
 * @param encoding used for @a contents data. If @c NULL "UTF-8" is assumed.
 * @param base_uri base uri to use for relative resources. May be @c NULL.
 *        If provided must be an absolute uri.
 *
 * @return @c EINA_TRUE on successful request, @c EINA_FALSE on errors.
 */
Eina_Bool ewk_frame_contents_set(Evas_Object* o, const char* contents, size_t contents_size, const char* mime_type, const char* encoding, const char* base_uri)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(contents, EINA_FALSE);
    return _ewk_frame_contents_set_internal
        (sd, contents, contents_size, mime_type, encoding, base_uri, 0);
}

/**
 * Requests loading alternative contents for unreachable URI in this frame.
 *
 * This is similar to ewk_frame_contents_set(), but is used when some
 * URI failed to load, using the provided content instead. The main
 * difference is that back-forward navigation list is not changed.
 *
 * @param o frame object to load document.
 * @param contents what to load into frame. Must not be @c NULL.
 * @param contents_size byte size of data in @a contents.
 *        If zero, strlen() is used.
 * @param mime_type type of @a contents data. If @c NULL "text/html" is assumed.
 * @param encoding used for @a contents data. If @c NULL "UTF-8" is assumed.
 * @param base_uri base uri to use for relative resources. May be @c NULL.
 *        If provided must be an absolute uri.
 * @param unreachable_uri the URI that failed to load and is getting the
 *        alternative representation.
 *
 * @return @c EINA_TRUE on successful request, @c EINA_FALSE on errors.
 */
Eina_Bool ewk_frame_contents_alternate_set(Evas_Object* o, const char* contents, size_t contents_size, const char* mime_type, const char* encoding, const char* base_uri, const char* unreachable_uri)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(contents, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(unreachable_uri, EINA_FALSE);
    return _ewk_frame_contents_set_internal
        (sd, contents, contents_size, mime_type, encoding, base_uri,
         unreachable_uri);
}

/**
 * Requests execution of given script.
 *
 * @param o frame object to execute script.
 * @param script java script to execute.
 *
 * @return @c EINA_TRUE if request was done, @c EINA_FALSE on errors.
 */
Eina_Bool ewk_frame_script_execute(Evas_Object* o, const char* script)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(script, EINA_FALSE);
    sd->frame->script()->executeScript(WTF::String::fromUTF8(script), true);
    return EINA_TRUE;
}

/**
 * Gets if frame is editable.
 *
 * @param o frame object to get editable state.
 *
 * @return @c EINA_TRUE if editable, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_editable_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return sd->editable;
}

/**
 * Sets if frame is editable.
 *
 * @param o frame object to set editable state.
 * @param editable new state.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_editable_set(Evas_Object* o, Eina_Bool editable)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    editable = !!editable;
    if (sd->editable == editable)
        return EINA_TRUE;
    if (editable)
        sd->frame->applyEditingStyleToBodyElement();
    return EINA_TRUE;
}

/**
 * Get the copy of the selection text.
 *
 * @param o frame object to get selection text.
 *
 * @return newly allocated string or @c NULL if nothing is selected or failure.
 */
char* ewk_frame_selection_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, 0);
    WTF::CString s = sd->frame->selectedText().utf8();
    if (s.isNull())
        return 0;
    return strdup(s.data());
}

static inline Eina_Bool _ewk_frame_editor_command(Ewk_Frame_Smart_Data* sd, const char* command)
{
    return sd->frame->editor()->command(WTF::String::fromUTF8(command)).execute();
}

/**
 * Unselects whatever was selected.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_none(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "Unselect");
}

/**
 * Selects everything.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_all(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "SelectAll");
}

/**
 * Selects the current paragrah.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_paragraph(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "SelectParagraph");
}

/**
 * Selects the current sentence.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_sentence(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "SelectSentence");
}

/**
 * Selects the current line.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_line(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "SelectLine");
}

/**
 * Selects the current word.
 *
 * @return @c EINA_TRUE if operation was executed, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_select_word(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return _ewk_frame_editor_command(sd, "SelectWord");
}

/**
 * Search the given text string in document.
 *
 * @param o frame object where to search text.
 * @param string reference string to search.
 * @param case_sensitive if search should be case sensitive or not.
 * @param forward if search is from cursor and on or backwards.
 * @param wrap if search should wrap at end.
 *
 * @return @c EINA_TRUE if found, @c EINA_FALSE if not or failure.
 */
Eina_Bool ewk_frame_text_search(const Evas_Object* o, const char* string, Eina_Bool case_sensitive, Eina_Bool forward, Eina_Bool wrap)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, EINA_FALSE);

    return sd->frame->findString(WTF::String::fromUTF8(string), forward, case_sensitive, wrap, true);
}

/**
 * Mark matches the given text string in document.
 *
 * @param o frame object where to search text.
 * @param string reference string to match.
 * @param case_sensitive if match should be case sensitive or not.
 * @param highlight if matches should be highlighted.
 * @param limit maximum amount of matches, or zero to unlimited.
 *
 * @return number of matches.
 */
unsigned int ewk_frame_text_matches_mark(Evas_Object* o, const char* string, Eina_Bool case_sensitive, Eina_Bool highlight, unsigned int limit)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(string, 0);

    sd->frame->setMarkedTextMatchesAreHighlighted(highlight);
    return sd->frame->markAllMatchesForText(WTF::String::fromUTF8(string), case_sensitive, limit);
}

/**
 * Reverses the effect of ewk_frame_text_matches_mark()
 *
 * @param o frame object where to search text.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE for failure.
 */
Eina_Bool ewk_frame_text_matches_unmark_all(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);

    sd->frame->document()->markers()->removeMarkers(WebCore::DocumentMarker::TextMatch);
    return EINA_TRUE;
}

/**
 * Set if should highlight matches marked with ewk_frame_text_matches_mark().
 *
 * @param o frame object where to set if matches are highlighted or not.
 * @param highlight if @c EINA_TRUE, matches will be highlighted.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE for failure.
 */
Eina_Bool ewk_frame_text_matches_highlight_set(Evas_Object* o, Eina_Bool highlight)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    sd->frame->setMarkedTextMatchesAreHighlighted(highlight);
    return EINA_TRUE;
}

/**
 * Get if should highlight matches marked with ewk_frame_text_matches_mark().
 *
 * @param o frame object to query if matches are highlighted or not.
 *
 * @return @c EINA_TRUE if they are highlighted, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_text_matches_highlight_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    return sd->frame->markedTextMatchesAreHighlighted();
}

/** 
 * Comparison function used by ewk_frame_text_matches_nth_pos_get
 */
static bool _ewk_frame_rect_cmp_less_than(const WebCore::IntRect& i, const WebCore::IntRect& j)
{
    return (i.y() < j.y() || (i.y() == j.y() && i.x() < j.x()));
}   
    
/**
 * Predicate used by ewk_frame_text_matches_nth_pos_get
 */
static bool _ewk_frame_rect_is_negative_value(const WebCore::IntRect& i)
{
    return (i.x() < 0 || i.y() < 0);
}

/**
 * Get x, y position of n-th text match in frame
 *
 * @param o frame object where matches are highlighted.
 * @param n index of element 
 * @param x where to return x position. May be @c NULL. 
 * @param y where to return y position. May be @c NULL.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE for failure - when no matches found or 
 *         n bigger than search results.
 */
Eina_Bool ewk_frame_text_matches_nth_pos_get(Evas_Object* o, int n, int* x, int* y)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);

    Vector<WebCore::IntRect> intRects = sd->frame->document()->markers()->renderedRectsForMarkers(WebCore::DocumentMarker::TextMatch);

    /* remove useless values */
    std::remove_if(intRects.begin(), intRects.end(), _ewk_frame_rect_is_negative_value);

    if (intRects.isEmpty() || n > intRects.size())
      return EINA_FALSE;

    std::sort(intRects.begin(), intRects.end(), _ewk_frame_rect_cmp_less_than);

    if (x)
      *x = intRects[n - 1].x();
    if (y)
      *y = intRects[n - 1].y();
    return EINA_TRUE;
}

/**
 * Ask frame to stop loading.
 *
 * @param o frame object to stop loading.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_stop(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    sd->frame->loader()->stopAllLoaders();
    return EINA_TRUE;
}

/**
 * Ask frame to reload current document.
 *
 * @param o frame object to reload.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_reload_full()
 */
Eina_Bool ewk_frame_reload(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    sd->frame->loader()->reload();
    return EINA_TRUE;
}

/**
 * Ask frame to fully reload current document, using no previous caches.
 *
 * @param o frame object to reload.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_reload_full(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    sd->frame->loader()->reload(true);
    return EINA_TRUE;
}

/**
 * Ask frame to navigate back in history.
 *
 * @param o frame object to navigate back.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_navigate()
 */
Eina_Bool ewk_frame_back(Evas_Object* o)
{
    return ewk_frame_navigate(o, -1);
}

/**
 * Ask frame to navigate forward in history.
 *
 * @param o frame object to navigate forward.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_navigate()
 */
Eina_Bool ewk_frame_forward(Evas_Object* o)
{
    return ewk_frame_navigate(o, 1);
}

/**
 * Navigate back or forward in history.
 *
 * @param o frame object to navigate.
 * @param steps if positive navigates that amount forwards, if negative
 *        does backwards.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_navigate(Evas_Object* o, int steps)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WebCore::Page* page = sd->frame->page();
    if (!page->canGoBackOrForward(steps))
        return EINA_FALSE;
    page->goBackOrForward(steps);
    return EINA_TRUE;
}

/**
 * Check if it is possible to navigate backwards one item in history.
 *
 * @param o frame object to check if backward navigation is possible.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_navigate_possible()
 */
Eina_Bool ewk_frame_back_possible(Evas_Object* o)
{
    return ewk_frame_navigate_possible(o, -1);
}

/**
 * Check if it is possible to navigate forwards one item in history.
 *
 * @param o frame object to check if forward navigation is possible.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 *
 * @see ewk_frame_navigate_possible()
 */
Eina_Bool ewk_frame_forward_possible(Evas_Object* o)
{
    return ewk_frame_navigate_possible(o, 1);
}

/**
 * Check if it is possible to navigate given @a steps in history.
 *
 * @param o frame object to navigate.
 * @param steps if positive navigates that amount forwards, if negative
 *        does backwards.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_navigate_possible(Evas_Object* o, int steps)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WebCore::Page* page = sd->frame->page();
    return page->canGoBackOrForward(steps);
}

/**
 * Get current zoom level used by this frame.
 *
 * @param o frame object to query zoom level.
 *
 * @return zoom level or -1.0 on failure.
 */
float ewk_frame_zoom_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, -1.0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, -1.0);
    WebCore::FrameView* view = sd->frame->view();
    if (!view)
        return -1;
    return view->zoomFactor();
}

/**
 * Set current zoom level used by this frame.
 *
 * @param o frame object to change zoom level.
 * @param zoom new level.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE on failure.
 *
 * @see ewk_frame_zoom_text_only_set()
 */
Eina_Bool ewk_frame_zoom_set(Evas_Object* o, float zoom)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WebCore::FrameView* view = sd->frame->view();
    if (!view)
        return EINA_FALSE;
    view->setZoomFactor(zoom, sd->zoom_mode);
    return EINA_TRUE;
}

/**
 * Query if zoom level just applies to text and not other elements.
 *
 * @param o frame to query setting.
 *
 * @return @c EINA_TRUE if just text are scaled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_zoom_text_only_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    return sd->zoom_mode == WebCore::ZoomTextOnly;
}

/**
 * Set if zoom level just applies to text and not other elements.
 *
 * @param o frame to change setting.
 * @param setting @c EINA_TRUE if zoom should just be applied to text.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_zoom_text_only_set(Evas_Object* o, Eina_Bool setting)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WebCore::ZoomMode zm = setting ? WebCore::ZoomTextOnly : WebCore::ZoomPage;
    if (sd->zoom_mode == zm)
        return EINA_TRUE;

    sd->zoom_mode = zm;
    WebCore::FrameView* view = sd->frame->view();
    if (!view)
        return EINA_FALSE;
    view->setZoomFactor(view->zoomFactor(), sd->zoom_mode);
    return EINA_TRUE;
}

/**
 * Free hit test created with ewk_frame_hit_test_new().
 *
 * @param hit_test instance. Must @b not be @c NULL.
 */
void ewk_frame_hit_test_free(Ewk_Hit_Test* hit_test)
{
    EINA_SAFETY_ON_NULL_RETURN(hit_test);
    eina_stringshare_del(hit_test->title);
    eina_stringshare_del(hit_test->alternate_text);
    eina_stringshare_del(hit_test->link.text);
    eina_stringshare_del(hit_test->link.url);
    eina_stringshare_del(hit_test->link.title);
    free(hit_test);
}

/**
 * Creates a new hit test for given frame and point.
 *
 * @param o frame to do hit test on.
 * @param x horizontal position to query.
 * @param y vertical position to query.
 *
 * @return a newly allocated hit test on success, @c NULL otherwise.
 *         Free memory with ewk_frame_hit_test_free()
 */
Ewk_Hit_Test* ewk_frame_hit_test_new(const Evas_Object* o, int x, int y)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, 0);

    WebCore::FrameView* view = sd->frame->view();
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->contentRenderer(), 0);

    WebCore::HitTestResult result = sd->frame->eventHandler()->hitTestResultAtPoint
        (view->windowToContents(WebCore::IntPoint(x, y)),
         /*allowShadowContent*/ false, /*ignoreClipping*/ true);

    if (result.scrollbar())
        return 0;
    if (!result.innerNode())
        return 0;

    Ewk_Hit_Test* hit_test = (Ewk_Hit_Test*)calloc(1, sizeof(Ewk_Hit_Test));
    if (!hit_test) {
        CRITICAL("Could not allocate memory for hit test.");
        return 0;
    }

    hit_test->x = result.point().x();
    hit_test->y = result.point().y();
#if 0
    // FIXME
    hit_test->bounding_box.x = result.boundingBox().x();
    hit_test->bounding_box.y = result.boundingBox().y();
    hit_test->bounding_box.w = result.boundingBox().width();
    hit_test->bounding_box.h = result.boundingBox().height();
#else
    hit_test->bounding_box.x = 0;
    hit_test->bounding_box.y = 0;
    hit_test->bounding_box.w = 0;
    hit_test->bounding_box.h = 0;
#endif

    WebCore::TextDirection dir;
    hit_test->title = eina_stringshare_add(result.title(dir).utf8().data());
    hit_test->alternate_text = eina_stringshare_add(result.altDisplayString().utf8().data());
    if (result.innerNonSharedNode() && result.innerNonSharedNode()->document()
        && result.innerNonSharedNode()->document()->frame())
        hit_test->frame = kit(result.innerNonSharedNode()->document()->frame());

    hit_test->link.text = eina_stringshare_add(result.textContent().utf8().data());
    hit_test->link.url = eina_stringshare_add(result.absoluteLinkURL().prettyURL().utf8().data());
    hit_test->link.title = eina_stringshare_add(result.titleDisplayString().utf8().data());
    hit_test->link.target_frame = kit(result.targetFrame());

    hit_test->flags.editable = result.isContentEditable();
    hit_test->flags.selected = result.isSelected();

    return hit_test;
}

/**
 * Relative scroll of given frame.
 *
 * @param o frame object to scroll.
 * @param dx horizontal offset to scroll.
 * @param dy vertical offset to scroll.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 */
Eina_Bool
ewk_frame_scroll_add(Evas_Object* o, int dx, int dy)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    sd->frame->view()->scrollBy(IntSize(dx, dy));
    return EINA_TRUE;
}

/**
 * Set absolute scroll of given frame.
 *
 * Both values are from zero to the contents size minus the viewport
 * size. See ewk_frame_scroll_size_get().
 *
 * @param o frame object to scroll.
 * @param x horizontal position to scroll.
 * @param y vertical position to scroll.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise.
 */
Eina_Bool
ewk_frame_scroll_set(Evas_Object* o, int x, int y)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    sd->frame->view()->setScrollPosition(WebCore::IntPoint(x, y));
    return EINA_TRUE;
}

/**
 * Get the possible scroll size of given frame.
 *
 * Possible scroll size is contents size minus the viewport
 * size. It's the last allowed value for ewk_frame_scroll_set()
 *
 * @param o frame object to scroll.
 * @param w where to return horizontal size that is possible to
 *        scroll. May be @c NULL.
 * @param h where to return vertical size that is possible to scroll.
 *        May be @c NULL.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise and
 *         values are zeroed.
 */
Eina_Bool
ewk_frame_scroll_size_get(const Evas_Object* o, int* w, int* h)
{
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    WebCore::IntPoint point = sd->frame->view()->maximumScrollPosition();
    if (w)
        *w = point.x();
    if (h)
        *h = point.y();
    return EINA_TRUE;
}

/**
 * Get the current scroll position of given frame.
 *
 * @param o frame object to scroll.
 * @param x where to return horizontal position. May be @c NULL.
 * @param y where to return vertical position. May be @c NULL.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise and
 *         values are zeroed.
 */
Eina_Bool
ewk_frame_scroll_pos_get(const Evas_Object* o, int* x, int* y)
{
    if (x)
        *x = 0;
    if (y)
        *y = 0;
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    WebCore::IntPoint pos = sd->frame->view()->scrollPosition();
    if (x)
        *x = pos.x();
    if (y)
        *y = pos.y();
    return EINA_TRUE;
}

/**
 * Get the current frame visible content geometry.
 *
 * @param o frame object to query visible content geometry.
 * @param include_scrollbars whenever to include scrollbars size.
 * @param x horizontal position. May be @c NULL.
 * @param y vertical position. May be @c NULL.
 * @param w width. May be @c NULL.
 * @param h height. May be @c NULL.
 *
 * @return @c EINA_TRUE if possible, @c EINA_FALSE otherwise and
 *         values are zeroed.
 */
Eina_Bool ewk_frame_visible_content_geometry_get(const Evas_Object* o, Eina_Bool include_scrollbars, int* x, int* y, int* w, int* h)
{
    if (x)
        *x = 0;
    if (y)
        *y = 0;
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    WebCore::IntRect rect = sd->frame->view()->visibleContentRect(include_scrollbars);
    if (x)
        *x = rect.x();
    if (y)
        *y = rect.y();
    if (w)
        *w = rect.width();
    if (h)
        *h = rect.height();
    return EINA_TRUE;
}

/**
 * Get the current paintsEntireContents flag.
 *
 * This flag tells if dirty areas should be repainted even if they are
 * out of the screen.
 *
 * @param o frame object to query paintsEntireContents flag.
 *
 * @return @c EINA_TRUE if repainting any dirty area, @c EINA_FALSE
 *         otherwise.
 */
Eina_Bool ewk_frame_paint_full_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame->view(), EINA_FALSE);
    return sd->frame->view()->paintsEntireContents();
}

/**
 * Set the current paintsEntireContents flag.
 *
 * This flag tells if dirty areas should be repainted even if they are
 * out of the screen.
 *
 * @param o frame object to set paintsEntireContents flag.
 * @param flag @c EINA_TRUE if want to always repaint any dirty area,
 *             @c EINA_FALSE otherwise.
 */
void ewk_frame_paint_full_set(Evas_Object* o, Eina_Bool flag)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->frame);
    EINA_SAFETY_ON_NULL_RETURN(sd->frame->view());
    sd->frame->view()->setPaintsEntireContents(flag);
}

/**
 * Feed the focus in signal to this frame.
 *
 * @param o frame object to focus.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_focus_in(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WebCore::FocusController* c = sd->frame->page()->focusController();
    c->setFocusedFrame(sd->frame);
    return EINA_TRUE;
}

/**
 * Feed the focus out signal to this frame.
 *
 * @param o frame object to remove focus.
 */
Eina_Bool ewk_frame_feed_focus_out(Evas_Object* o)
{
    // TODO: what to do on focus out?
    ERR("what to do?");
    return EINA_FALSE;
}

/**
 * Feed the mouse wheel event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev mouse wheel event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_mouse_wheel(Evas_Object* o, const Evas_Event_Mouse_Wheel* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    WebCore::FrameView* view = sd->frame->view();
    DBG("o=%p, view=%p, direction=%d, z=%d, pos=%d,%d",
        o, view, ev->direction, ev->z, ev->canvas.x, ev->canvas.y);
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    WebCore::PlatformWheelEvent event(ev);
    return sd->frame->eventHandler()->handleWheelEvent(event);
}

/**
 * Feed the mouse down event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev mouse down event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_mouse_down(Evas_Object* o, const Evas_Event_Mouse_Down* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    WebCore::FrameView* view = sd->frame->view();
    DBG("o=%p, view=%p, button=%d, pos=%d,%d",
        o, view, ev->button, ev->canvas.x, ev->canvas.y);
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    Evas_Coord x, y;
    evas_object_geometry_get(sd->view, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(ev, WebCore::IntPoint(x, y));
    return sd->frame->eventHandler()->handleMousePressEvent(event);
}

/**
 * Feed the mouse up event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev mouse up event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_mouse_up(Evas_Object* o, const Evas_Event_Mouse_Up* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    WebCore::FrameView* view = sd->frame->view();
    DBG("o=%p, view=%p, button=%d, pos=%d,%d",
        o, view, ev->button, ev->canvas.x, ev->canvas.y);
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    Evas_Coord x, y;
    evas_object_geometry_get(sd->view, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(ev, WebCore::IntPoint(x, y));
    return sd->frame->eventHandler()->handleMouseReleaseEvent(event);
}

/**
 * Feed the mouse move event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev mouse move event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_mouse_move(Evas_Object* o, const Evas_Event_Mouse_Move* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    WebCore::FrameView* view = sd->frame->view();
    DBG("o=%p, view=%p, pos: old=%d,%d, new=%d,%d, buttons=%d",
        o, view, ev->cur.canvas.x, ev->cur.canvas.y,
        ev->prev.canvas.x, ev->prev.canvas.y, ev->buttons);
    EINA_SAFETY_ON_NULL_RETURN_VAL(view, EINA_FALSE);

    Evas_Coord x, y;
    evas_object_geometry_get(sd->view, &x, &y, 0, 0);

    WebCore::PlatformMouseEvent event(ev, WebCore::IntPoint(x, y));
    return sd->frame->eventHandler()->mouseMoved(event);
}

static inline Eina_Bool _ewk_frame_handle_key_scrolling(WebCore::Frame* frame, const WebCore::PlatformKeyboardEvent &event)
{
    WebCore::ScrollDirection direction;
    WebCore::ScrollGranularity granularity;

    int keyCode = event.windowsVirtualKeyCode();

    switch (keyCode) {
    case VK_SPACE:
        granularity = WebCore::ScrollByPage;
        if (event.shiftKey())
            direction = WebCore::ScrollUp;
        else
            direction = WebCore::ScrollDown;
        break;
    case VK_NEXT:
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollDown;
        break;
    case VK_PRIOR:
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollUp;
        break;
    case VK_HOME:
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollUp;
        break;
    case VK_END:
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollDown;
        break;
    case VK_LEFT:
        granularity = WebCore::ScrollByLine;
        direction = WebCore::ScrollLeft;
        break;
    case VK_RIGHT:
        granularity = WebCore::ScrollByLine;
        direction = WebCore::ScrollRight;
        break;
    case VK_UP:
        direction = WebCore::ScrollUp;
        if (event.ctrlKey())
            granularity = WebCore::ScrollByDocument;
        else
            granularity = WebCore::ScrollByLine;
        break;
    case VK_DOWN:
        direction = WebCore::ScrollDown;
         if (event.ctrlKey())
            granularity = WebCore::ScrollByDocument;
        else
            granularity = WebCore::ScrollByLine;
        break;
    default:
        return EINA_FALSE;
    }

    if (frame->eventHandler()->scrollOverflow(direction, granularity))
        return EINA_FALSE;

    frame->view()->scroll(direction, granularity);
    return EINA_TRUE;
}

/**
 * Feed the keyboard key down event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev keyboard key down event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_key_down(Evas_Object* o, const Evas_Event_Key_Down* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    DBG("o=%p keyname=%s (key=%s, string=%s)",
        o, ev->keyname, ev->key ? ev->key : "", ev->string ? ev->string : "");

    WebCore::PlatformKeyboardEvent event(ev);
    if (sd->frame->eventHandler()->keyEvent(event))
        return EINA_TRUE;

    return _ewk_frame_handle_key_scrolling(sd->frame, event);
}

/**
 * Feed the keyboard key up event to the frame.
 *
 * @param o frame object to feed event.
 * @param ev keyboard key up event.
 *
 * @return @c EINA_TRUE if it was handled, @c EINA_FALSE otherwise.
 */
Eina_Bool ewk_frame_feed_key_up(Evas_Object* o, const Evas_Event_Key_Up* ev)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ev, EINA_FALSE);

    DBG("o=%p keyname=%s (key=%s, string=%s)",
        o, ev->keyname, ev->key ? ev->key : "", ev->string ? ev->string : "");

    WebCore::PlatformKeyboardEvent event(ev);
    return sd->frame->eventHandler()->keyEvent(event);
}

/* internal methods ****************************************************/

/**
 * @internal
 *
 * Initialize frame based on actual WebKit frame.
 *
 * This is internal and should never be called by external users.
 */
Eina_Bool ewk_frame_init(Evas_Object* o, Evas_Object* view, WebCore::Frame* frame)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    if (!sd->frame) {
        WebCore::FrameLoaderClientEfl* flc = _ewk_frame_loader_efl_get(frame);
        flc->setWebFrame(o);
        sd->frame = frame;
        sd->view = view;
        frame->init();
        return EINA_TRUE;
    }

    ERR("frame %p already set for %p, ignored new %p",
        sd->frame, o, frame);
    return EINA_FALSE;
}

Evas_Object* ewk_frame_child_add(Evas_Object* o, WTF::PassRefPtr<WebCore::Frame> child, const WTF::String& name, const WebCore::KURL& url, const WTF::String& referrer)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    char buf[256];
    Evas_Object* frame;
    WebCore::Frame* cf;

    frame = ewk_frame_add(sd->base.evas);
    if (!frame) {
        ERR("Could not create ewk_frame object.");
        return 0;
    }

    cf = child.get();
    sd->frame->tree()->appendChild(child);
    if (cf->tree())
        cf->tree()->setName(name);
    else
        ERR("no tree for child object");

    if (!ewk_frame_init(frame, sd->view, cf)) {
        evas_object_del(frame);
        return 0;
    }
    snprintf(buf, sizeof(buf), "EWK_Frame:child/%s", name.utf8().data());
    evas_object_name_set(frame, buf);
    evas_object_smart_member_add(frame, o);
    evas_object_show(frame);

    if (!cf->page())
        goto died;

    cf->loader()->loadURLIntoChildFrame(url, referrer, cf);
    if (!cf->tree()->parent())
        goto died;

    // TODO: announce frame was created?
    return frame;

died:
    CRITICAL("does this work: BEGIN");
    ewk_frame_core_gone(frame); // CONFIRM
    evas_object_del(frame); // CONFIRM
    CRITICAL("does this work: END");
    return 0;
}

/**
 * @internal
 * Frame was destroyed by loader, remove internal reference.
 */
void ewk_frame_core_gone(Evas_Object* o)
{
    DBG("o=%p", o);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    sd->frame = 0;
}

/**
 * @internal
 * Retrieve WebCore::Frame associated with this object.
 *
 * Avoid using this call from outside, add specific ewk_frame_*
 * actions instead.
 */
WebCore::Frame* ewk_frame_core_get(const Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, 0);
    return sd->frame;
}


/**
 * @internal
 * Reports a resource will be requested. User may override behavior of webkit by
 * changing values in @param request.
 *
 * @param o Frame.
 * @param request Request details that user may override. Whenever values on
 * this struct changes, it must be properly malloc'd as it will be freed
 * afterwards.
 *
 * Emits signal: "resource,request,willsend"
 */
void ewk_frame_request_will_send(Evas_Object *o, Ewk_Frame_Resource_Request *request)
{
    evas_object_smart_callback_call(o, "resource,request,willsend", request);
}

/**
 * @internal
 * Reports that there's a new resource.
 *
 * @param o Frame.
 * @param request New request details. No changes are allowed to fields.
 *
 * Emits signal: "resource,request,new"
 */
void ewk_frame_request_assign_identifier(Evas_Object *o, const Ewk_Frame_Resource_Request *request)
{
    evas_object_smart_callback_call(o, "resource,request,new", (void *)request);
}

/**
 * @internal
 * Reports that first navigation occurred
 *
 * @param o Frame.
 *
 * Emits signal: "navigation,first"
 */
void ewk_frame_did_perform_first_navigation(Evas_Object *o)
{
    evas_object_smart_callback_call(o, "navigation,first", 0);
}

/**
 * @internal
 * Reports frame will be saved to current state
 *
 * @param o Frame.
 * @param item History item to save details to.
 *
 * Emits signal: "state,save"
 */
void ewk_frame_view_state_save(Evas_Object *o, WebCore::HistoryItem* item)
{
    evas_object_smart_callback_call(o, "state,save", 0);
}

/**
 * @internal
 * Reports the frame started loading something.
 *
 * Emits signal: "load,started" with no parameters.
 */
void ewk_frame_load_started(Evas_Object* o)
{
    Evas_Object* main_frame;
    DBG("o=%p", o);
    evas_object_smart_callback_call(o, "load,started", 0);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    ewk_view_load_started(sd->view);

    main_frame = ewk_view_frame_main_get(sd->view);
    if (main_frame == o)
        ewk_view_frame_main_load_started(sd->view);
}

/**
 * @internal
 * Reports the frame started provisional load.
 *
 * @param o Frame.
 *
 * Emits signal: "load,provisional" with no parameters.
 */
void ewk_frame_load_provisional(Evas_Object* o)
{
    evas_object_smart_callback_call(o, "load,provisional", 0);
}

/**
 * @internal
 * Reports the frame finished first layout.
 *
 * @param o Frame.
 *
 * Emits signal: "load,firstlayout,finished" with no parameters.
 */
void ewk_frame_load_firstlayout_finished(Evas_Object *o)
{
    evas_object_smart_callback_call(o, "load,firstlayout,finished", 0);
}

/**
 * @internal
 * Reports the frame finished first non empty layout.
 *
 * @param o Frame.
 *
 * Emits signal: "load,nonemptylayout,finished" with no parameters.
 */
void ewk_frame_load_firstlayout_nonempty_finished(Evas_Object *o)
{
    evas_object_smart_callback_call(o, "load,nonemptylayout,finished", 0);
}

/**
 * @internal
 * Reports the loading of a document has finished on frame.
 *
 * @param o Frame.
 *
 * Emits signal: "load,document,finished" with no parameters.
 */
void ewk_frame_load_document_finished(Evas_Object *o)
{
    evas_object_smart_callback_call(o, "load,document,finished", 0);
}

/**
 * @internal
 * Reports load finished, optionally with error information.
 *
 * Emits signal: "load,finished" with pointer to Ewk_Frame_Load_Error
 * if any error, or @c NULL if successful load.
 *
 * @note there should notbe any error stuff here, but trying to be
 *       compatible with previous WebKit.
 */
void ewk_frame_load_finished(Evas_Object* o, const char* error_domain, int error_code, Eina_Bool is_cancellation, const char* error_description, const char* failing_url)
{
    Ewk_Frame_Load_Error buf, *error;
    if (!error_domain) {
        DBG("o=%p, success.", o);
        error = 0;
    } else {
        DBG("o=%p, error=%s (%d, cancellation=%hhu) \"%s\", url=%s",
            o, error_domain, error_code, is_cancellation,
            error_description, failing_url);

        buf.domain = error_domain;
        buf.code = error_code;
        buf.is_cancellation = is_cancellation;
        buf.description = error_description;
        buf.failing_url = failing_url;
        buf.frame = o;
        error = &buf;
    }
    evas_object_smart_callback_call(o, "load,finished", error);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    ewk_view_load_finished(sd->view, error);
}

/**
 * @internal
 * Reports load failed with error information.
 *
 * Emits signal: "load,error" with pointer to Ewk_Frame_Load_Error.
 */
void ewk_frame_load_error(Evas_Object* o, const char* error_domain, int error_code, Eina_Bool is_cancellation, const char* error_description, const char* failing_url)
{
    Ewk_Frame_Load_Error error;

    DBG("o=%p, error=%s (%d, cancellation=%hhu) \"%s\", url=%s",
        o, error_domain, error_code, is_cancellation,
        error_description, failing_url);

    EINA_SAFETY_ON_NULL_RETURN(error_domain);

    error.code = error_code;
    error.is_cancellation = is_cancellation;
    error.domain = error_domain;
    error.description = error_description;
    error.failing_url = failing_url;
    error.frame = o;
    evas_object_smart_callback_call(o, "load,error", &error);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    ewk_view_load_error(sd->view, &error);
}

/**
 * @internal
 * Reports load progress changed.
 *
 * Emits signal: "load,progress" with pointer to a double from 0.0 to 1.0.
 */
void ewk_frame_load_progress_changed(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->frame);

    // TODO: this is per page, there should be a way to have per-frame.
    double progress = sd->frame->page()->progress()->estimatedProgress();

    DBG("o=%p (p=%0.3f)", o, progress);

    evas_object_smart_callback_call(o, "load,progress", &progress);
    ewk_view_load_progress_changed(sd->view);
}


/**
 * @internal
 *
 * Reports contents size changed.
 */
void ewk_frame_contents_size_changed(Evas_Object* o, Evas_Coord w, Evas_Coord h)
{
    DBG("o=%p: %dx%d", o, w, h);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    if (sd->contents_size.w == w && sd->contents_size.h == h)
        return;
    sd->contents_size.w = w;
    sd->contents_size.h = h;
    // TODO: update something else internally?

    Evas_Coord size[2] = {w, h};
    evas_object_smart_callback_call(o, "contents,size,changed", size);
}

/**
 * @internal
 *
 * Reports title changed.
 */
void ewk_frame_title_set(Evas_Object* o, const char* title)
{
    DBG("o=%p, title=%s", o, title ? title : "(null)");
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    if (!eina_stringshare_replace(&sd->title, title))
        return;
    evas_object_smart_callback_call(o, "title,changed", (void*)sd->title);
}

void ewk_frame_view_create_for_view(Evas_Object* o, Evas_Object* view)
{
    DBG("o=%p, view=%p", o, view);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->frame);
    Evas_Coord w, h;

    if (sd->frame->view())
        return;

    evas_object_geometry_get(view, 0, 0, &w, &h);

    WebCore::IntSize size(w, h);
    int r, g, b, a;
    WebCore::Color bg;

    ewk_view_bg_color_get(view, &r, &g, &b, &a);
    if (!a)
        bg = WebCore::Color(0, 0, 0, 0);
    else if (a == 255)
        bg = WebCore::Color(r, g, b, a);
    else
        bg = WebCore::Color(r * 255 / a, g * 255 / a, b * 255 / a, a);

    sd->frame->createView(size, bg, !a, WebCore::IntSize(), false);
    if (!sd->frame->view())
        return;
    sd->frame->view()->setEdjeTheme(sd->theme);
    sd->frame->view()->setEvasObject(o);
}

/**
 * @internal
 * Reports uri changed and swap internal string reference.
 *
 * Emits signal: "uri,changed" with new uri as parameter.
 */
Eina_Bool ewk_frame_uri_changed(Evas_Object* o)
{
    EWK_FRAME_SD_GET_OR_RETURN(o, sd, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd->frame, EINA_FALSE);
    WTF::CString uri(sd->frame->loader()->url().prettyURL().utf8());

    INF("uri=%s", uri.data());
    if (!uri.data()) {
        ERR("no uri");
        return EINA_FALSE;
    }

    eina_stringshare_replace(&sd->uri, uri.data());
    evas_object_smart_callback_call(o, "uri,changed", (void*)sd->uri);
    return EINA_TRUE;
}

void ewk_frame_force_layout(Evas_Object* o)
{
    DBG("o=%p", o);
    EWK_FRAME_SD_GET_OR_RETURN(o, sd);
    EINA_SAFETY_ON_NULL_RETURN(sd->frame);
    WebCore::FrameView* view = sd->frame->view();
    if (view)
        view->forceLayout(true);
}

WTF::PassRefPtr<WebCore::Widget> ewk_frame_plugin_create(Evas_Object* o, const WebCore::IntSize& pluginSize, WebCore::HTMLPlugInElement* element, const WebCore::KURL& url, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues, const WTF::String& mimeType, bool loadManually)
{
    return 0;
}

/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef PlatformSupport_h
#define PlatformSupport_h

#if ENABLE(WEB_AUDIO)
#include "AudioBus.h"
#endif

#include "FileSystem.h"
#include "ImageSource.h"
#include "LinkHash.h"
#include "PluginData.h"

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

typedef struct NPObject NPObject;
typedef struct _NPP NPP_t;
typedef NPP_t* NPP;

#if OS(DARWIN)
typedef struct CGFont* CGFontRef;
#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif
#endif // OS(DARWIN)

#if OS(WINDOWS)
typedef struct HFONT__* HFONT;
#endif

namespace WebCore {

class Color;
class Cursor;
class Document;
class Frame;
class GeolocationServiceBridge;
class GeolocationServiceChromium;
class GraphicsContext;
class Image;
class IDBFactoryBackendInterface;
class IntRect;
class KURL;
class SerializedScriptValue;
class Widget;

struct Cookie;
struct FontRenderStyle;

// PlatformSupport an interface to the embedding layer that lets the embedder
// supply implementations for Platform functionality that WebCore cannot access
// directly (for example, because WebCore is in a sandbox).

class PlatformSupport {
public:
    // Cookies ------------------------------------------------------------
    static void setCookies(const Document*, const KURL&, const String& value);
    static String cookies(const Document*, const KURL&);
    static String cookieRequestHeaderFieldValue(const Document*, const KURL&);
    static bool rawCookies(const Document*, const KURL&, Vector<Cookie>&);
    static void deleteCookie(const Document*, const KURL&, const String& cookieName);
    static bool cookiesEnabled(const Document*);

    // Font ---------------------------------------------------------------
#if OS(WINDOWS)
    static bool ensureFontLoaded(HFONT);
#endif
#if OS(DARWIN)
    static bool loadFont(NSFont* srcFont, CGFontRef*, uint32_t* fontID);
#elif OS(UNIX)
    struct FontFamily {
        String name;
        bool isBold;
        bool isItalic;
    };
    static void getFontFamilyForCharacters(const UChar*, size_t numCharacters, const char* preferredLocale, FontFamily*);
#endif

    // Forms --------------------------------------------------------------
    static void notifyFormStateChanged(const Document*);

    // IndexedDB ----------------------------------------------------------
    static PassRefPtr<IDBFactoryBackendInterface> idbFactory();

    // JavaScript ---------------------------------------------------------
    static void notifyJSOutOfMemory(Frame*);
    static bool allowScriptDespiteSettings(const KURL& documentURL);

    // Plugin -------------------------------------------------------------
    static bool plugins(bool refresh, Vector<PluginInfo>*);
    static NPObject* pluginScriptableObject(Widget*);
    static bool popupsAllowed(NPP);

    // Screen -------------------------------------------------------------
    static int screenHorizontalDPI(Widget*);
    static int screenVerticalDPI(Widget*);
    static int screenDepth(Widget*);
    static int screenDepthPerComponent(Widget*);
    static bool screenIsMonochrome(Widget*);
    static IntRect screenRect(Widget*);
    static IntRect screenAvailableRect(Widget*);

    // Theming ------------------------------------------------------------
#if OS(WINDOWS)
    static void paintButton(
        GraphicsContext*, int part, int state, int classicState, const IntRect&);
    static void paintMenuList(
        GraphicsContext*, int part, int state, int classicState, const IntRect&);
    static void paintScrollbarArrow(
        GraphicsContext*, int state, int classicState, const IntRect&);
    static void paintScrollbarThumb(
        GraphicsContext*, int part, int state, int classicState, const IntRect&);
    static void paintScrollbarTrack(
        GraphicsContext*, int part, int state, int classicState, const IntRect&, const IntRect& alignRect);
    static void paintSpinButton(
        GraphicsContext*, int part, int state, int classicState, const IntRect&);
    static void paintTextField(
        GraphicsContext*, int part, int state, int classicState, const IntRect&, const Color&, bool fillContentArea, bool drawEdges);
    static void paintTrackbar(
        GraphicsContext*, int part, int state, int classicState, const IntRect&);
    static void paintProgressBar(
        GraphicsContext*, const IntRect& barRect, const IntRect& valueRect, bool determinate, double animatedSeconds);
#elif OS(DARWIN)
    enum ThemePaintState {
        StateDisabled,
        StateInactive,
        StateActive,
        StatePressed,
    };

    enum ThemePaintSize {
        SizeRegular,
        SizeSmall,
    };

    enum ThemePaintScrollbarOrientation {
        ScrollbarOrientationHorizontal,
        ScrollbarOrientationVertical,
    };

    enum ThemePaintScrollbarParent {
        ScrollbarParentScrollView,
        ScrollbarParentRenderLayer,
    };

    struct ThemePaintScrollbarInfo {
        ThemePaintScrollbarOrientation orientation;
        ThemePaintScrollbarParent parent;
        int maxValue;
        int currentValue;
        int visibleSize;
        int totalSize;
    };

    static void paintScrollbarThumb(GraphicsContext*, ThemePaintState, ThemePaintSize, const IntRect&, const ThemePaintScrollbarInfo&);
#elif OS(UNIX)
    // The UI part which is being accessed.
    enum ThemePart {
        // ScrollbarTheme parts
        PartScrollbarDownArrow,
        PartScrollbarLeftArrow,
        PartScrollbarRightArrow,
        PartScrollbarUpArrow,
        PartScrollbarHorizontalThumb,
        PartScrollbarVerticalThumb,
        PartScrollbarHorizontalTrack,
        PartScrollbarVerticalTrack,

        // RenderTheme parts
        PartCheckbox,
        PartRadio,
        PartButton,
        PartTextField,
        PartMenuList,
        PartSliderTrack,
        PartSliderThumb,
        PartInnerSpinButton,
        PartProgressBar
    };

    // The current state of the associated Part.
    enum ThemePaintState {
        StateDisabled,
        StateHover,
        StateNormal,
        StatePressed
    };

    struct ScrollbarTrackExtraParams {
        // The bounds of the entire track, as opposed to the part being painted.
        int trackX;
        int trackY;
        int trackWidth;
        int trackHeight;
    };

    struct ButtonExtraParams {
        bool checked;
        bool indeterminate; // Whether the button state is indeterminate.
        bool isDefault; // Whether the button is default button.
        bool hasBorder;
        unsigned backgroundColor;
    };

    struct TextFieldExtraParams {
        bool isTextArea;
        bool isListbox;
        unsigned backgroundColor;
    };

    struct MenuListExtraParams {
        bool hasBorder;
        bool hasBorderRadius;
        int arrowX;
        int arrowY;
        unsigned backgroundColor;
    };

    struct SliderExtraParams {
        bool vertical;
        bool inDrag;
    };

    struct InnerSpinButtonExtraParams {
        bool spinUp;
        bool readOnly;
    };

    struct ProgressBarExtraParams {
        bool determinate;
        int valueRectX;
        int valueRectY;
        int valueRectWidth;
        int valueRectHeight;
    };

    union ThemePaintExtraParams {
        ScrollbarTrackExtraParams scrollbarTrack;
        ButtonExtraParams button;
        TextFieldExtraParams textField;
        MenuListExtraParams menuList;
        SliderExtraParams slider;
        InnerSpinButtonExtraParams innerSpin;
        ProgressBarExtraParams progressBar;
    };

    // Gets the size of the given theme part. For variable sized items
    // like vertical scrollbar thumbs, the width will be the required width of
    // the track while the height will be the minimum height.
    static IntSize getThemePartSize(ThemePart);
    // Paint the given the given theme part.
    static void paintThemePart(GraphicsContext*, ThemePart, ThemePaintState, const IntRect&, const ThemePaintExtraParams*);
#endif
};

} // namespace WebCore

#endif

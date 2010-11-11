/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 * Copyright (C) 2010 Igalia S.L.
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
#include "RenderThemeGtk.h"

#include "AffineTransform.h"
#include "CSSValueKeywords.h"
#include "GOwnPtr.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GtkVersioning.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "NotImplemented.h"
#include "PlatformMouseEvent.h"
#include "RenderBox.h"
#include "RenderObject.h"
#include "Scrollbar.h"
#include "TimeRanges.h"
#include "UserAgentStyleSheets.h"
#include "WidgetRenderingContext.h"
#include "gtkdrawing.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <wtf/text/CString.h>

#if ENABLE(PROGRESS_TAG)
#include "RenderProgress.h"
#endif

namespace WebCore {

using namespace HTMLNames;

#if ENABLE(VIDEO)
static HTMLMediaElement* getMediaElementFromRenderObject(RenderObject* o)
{
    Node* node = o->node();
    Node* mediaNode = node ? node->shadowAncestorNode() : 0;
    if (!mediaNode || (!mediaNode->hasTagName(videoTag) && !mediaNode->hasTagName(audioTag)))
        return 0;

    return static_cast<HTMLMediaElement*>(mediaNode);
}

static gchar* getIconNameForTextDirection(const char* baseName)
{
    GString* nameWithDirection = g_string_new(baseName);
    GtkTextDirection textDirection = gtk_widget_get_default_direction();

    if (textDirection == GTK_TEXT_DIR_RTL)
        g_string_append(nameWithDirection, "-rtl");
    else if (textDirection == GTK_TEXT_DIR_LTR)
        g_string_append(nameWithDirection, "-ltr");

    return g_string_free(nameWithDirection, FALSE);
}

void RenderThemeGtk::initMediaStyling(GtkStyle* style, bool force)
{
    static bool stylingInitialized = false;

    if (!stylingInitialized || force) {
        m_panelColor = style->bg[GTK_STATE_NORMAL];
        m_sliderColor = style->bg[GTK_STATE_ACTIVE];
        m_sliderThumbColor = style->bg[GTK_STATE_SELECTED];

        // Names of these icons can vary because of text direction.
        gchar* playButtonIconName = getIconNameForTextDirection("gtk-media-play");
        gchar* seekBackButtonIconName = getIconNameForTextDirection("gtk-media-rewind");
        gchar* seekForwardButtonIconName = getIconNameForTextDirection("gtk-media-forward");

        m_fullscreenButton.clear();
        m_muteButton.clear();
        m_unmuteButton.clear();
        m_playButton.clear();
        m_pauseButton.clear();
        m_seekBackButton.clear();
        m_seekForwardButton.clear();

        m_fullscreenButton = Image::loadPlatformThemeIcon("gtk-fullscreen", m_mediaIconSize);
        // Note that the muteButton and unmuteButton take icons reflecting
        // the *current* state. Hence, the unmuteButton represents the *muted*
        // status, the muteButton represents the then current *unmuted* status.
        m_muteButton = Image::loadPlatformThemeIcon("audio-volume-high", m_mediaIconSize);
        m_unmuteButton = Image::loadPlatformThemeIcon("audio-volume-muted", m_mediaIconSize);
        m_playButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(playButtonIconName), m_mediaIconSize);
        m_pauseButton = Image::loadPlatformThemeIcon("gtk-media-pause", m_mediaIconSize);
        m_seekBackButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(seekBackButtonIconName), m_mediaIconSize);
        m_seekForwardButton = Image::loadPlatformThemeIcon(reinterpret_cast<const char*>(seekForwardButtonIconName), m_mediaIconSize);

        g_free(playButtonIconName);
        g_free(seekBackButtonIconName);
        g_free(seekForwardButtonIconName);
        stylingInitialized = true;
    }
}
#endif

PassRefPtr<RenderTheme> RenderThemeGtk::create()
{
    return adoptRef(new RenderThemeGtk());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeGtk::create().releaseRef();
    return rt;
}

static int mozGtkRefCount = 0;

RenderThemeGtk::RenderThemeGtk()
    : m_gtkWindow(0)
    , m_gtkContainer(0)
    , m_gtkButton(0)
    , m_gtkEntry(0)
    , m_gtkTreeView(0)
    , m_panelColor(Color::white)
    , m_sliderColor(Color::white)
    , m_sliderThumbColor(Color::white)
    , m_mediaIconSize(16)
    , m_mediaSliderHeight(14)
    , m_mediaSliderThumbWidth(12)
    , m_mediaSliderThumbHeight(12)
    , m_fullscreenButton(0)
    , m_muteButton(0)
    , m_unmuteButton(0)
    , m_playButton(0)
    , m_pauseButton(0)
    , m_seekBackButton(0)
    , m_seekForwardButton(0)
#ifdef GTK_API_VERSION_2
    , m_themePartsHaveRGBAColormap(true)
#endif
{

    memset(&m_themeParts, 0, sizeof(GtkThemeParts));
#ifdef GTK_API_VERSION_2
    GdkColormap* colormap = gdk_screen_get_rgba_colormap(gdk_screen_get_default());
    if (!colormap) {
        m_themePartsHaveRGBAColormap = false;
        colormap = gdk_screen_get_default_colormap(gdk_screen_get_default());
    }
    m_themeParts.colormap = colormap;
#endif

    // Initialize the Mozilla theme drawing code.
    if (!mozGtkRefCount) {
        moz_gtk_init();
        moz_gtk_use_theme_parts(&m_themeParts);
    }
    ++mozGtkRefCount;

#if ENABLE(VIDEO)
    initMediaStyling(gtk_rc_get_style(GTK_WIDGET(gtkContainer())), false);
#endif
}

RenderThemeGtk::~RenderThemeGtk()
{
    --mozGtkRefCount;

    if (!mozGtkRefCount)
        moz_gtk_shutdown();

    m_fullscreenButton.clear();
    m_muteButton.clear();
    m_unmuteButton.clear();
    m_playButton.clear();
    m_pauseButton.clear();
    m_seekBackButton.clear();
    m_seekForwardButton.clear();

    gtk_widget_destroy(m_gtkWindow);
}

void RenderThemeGtk::getIndicatorMetrics(ControlPart part, int& indicatorSize, int& indicatorSpacing) const
{
    ASSERT(part == CheckboxPart || part == RadioPart);
    if (part == CheckboxPart) {
        moz_gtk_checkbox_get_metrics(&indicatorSize, &indicatorSpacing);
        return;
    }

    // RadioPart
    moz_gtk_radio_get_metrics(&indicatorSize, &indicatorSpacing);
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
    case SliderHorizontalPart:
    case SliderVerticalPart:
        return true;
    default:
        return false;
    }
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

int RenderThemeGtk::baselinePosition(const RenderObject* o) const
{
    if (!o->isBox())
        return 0;

    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (o->style()->appearance() == CheckboxPart
        || o->style()->appearance() == RadioPart) {
        const RenderBox* box = toRenderBox(o);
        return box->marginTop() + box->height() - 2;
    }

    return RenderTheme::baselinePosition(o);
}

static GtkTextDirection gtkTextDirection(TextDirection direction)
{
    switch (direction) {
    case RTL:
        return GTK_TEXT_DIR_RTL;
    case LTR:
        return GTK_TEXT_DIR_LTR;
    default:
        return GTK_TEXT_DIR_NONE;
    }
}

static void adjustMozillaStyle(const RenderThemeGtk* theme, RenderStyle* style, GtkThemeWidgetType type)
{
    gint left, top, right, bottom;
    GtkTextDirection direction = gtkTextDirection(style->direction());
    gboolean inhtml = true;

    if (moz_gtk_get_widget_border(type, &left, &top, &right, &bottom, direction, inhtml) != MOZ_GTK_SUCCESS)
        return;

    // FIXME: This approach is likely to be incorrect. See other ports and layout tests to see the problem.
    const int xpadding = 1;
    const int ypadding = 1;

    style->setPaddingLeft(Length(xpadding + left, Fixed));
    style->setPaddingTop(Length(ypadding + top, Fixed));
    style->setPaddingRight(Length(xpadding + right, Fixed));
    style->setPaddingBottom(Length(ypadding + bottom, Fixed));
}

bool RenderThemeGtk::paintRenderObject(GtkThemeWidgetType type, RenderObject* renderObject, GraphicsContext* context, const IntRect& rect, int flags)
{
    // Painting is disabled so just claim to have succeeded
    if (context->paintingDisabled())
        return false;

    GtkWidgetState widgetState;
    widgetState.active = isPressed(renderObject);
    widgetState.focused = isFocused(renderObject);

    // https://bugs.webkit.org/show_bug.cgi?id=18364
    // The Mozilla theme drawing code, only paints a button as pressed when it's pressed 
    // while hovered. Until we move away from the Mozila code, work-around the issue by
    // forcing a pressed button into the hovered state. This ensures that buttons activated
    // via the keyboard have the proper rendering.
    widgetState.inHover = isHovered(renderObject) || (type == MOZ_GTK_BUTTON && isPressed(renderObject));

    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    widgetState.disabled = !isEnabled(renderObject) || isReadOnlyControl(renderObject);
    widgetState.isDefault = false;
    widgetState.canDefault = false;

    // FIXME: The depressed value should probably apply for other theme parts too.
    // It must be used for range thumbs, because otherwise when the thumb is pressed,
    // the rendering is incorrect.
    if (type == MOZ_GTK_SCALE_THUMB_HORIZONTAL || type == MOZ_GTK_SCALE_THUMB_VERTICAL)
        widgetState.depressed = isPressed(renderObject);
    else
        widgetState.depressed = false;

    WidgetRenderingContext widgetContext(context, rect);
    return !widgetContext.paintMozillaWidget(type, &widgetState, flags, gtkTextDirection(renderObject->style()->direction()));
}

static void setToggleSize(const RenderThemeGtk* theme, RenderStyle* style, ControlPart appearance)
{
    // The width and height are both specified, so we shouldn't change them.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME: This is probably not correct use of indicatorSize and indicatorSpacing.
    gint indicatorSize, indicatorSpacing;
    theme->getIndicatorMetrics(appearance, indicatorSize, indicatorSpacing);

    // Other ports hard-code this to 13, but GTK+ users tend to demand the native look.
    // It could be made a configuration option values other than 13 actually break site compatibility.
    int length = indicatorSize + indicatorSpacing;
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(length, Fixed));

    if (style->height().isAuto())
        style->setHeight(Length(length, Fixed));
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_CHECKBUTTON, o, i.context, rect, isChecked(o));
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    setToggleSize(this, style, RadioPart);
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_RADIOBUTTON, o, i.context, rect, isChecked(o));
}

void RenderThemeGtk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Some layout tests check explicitly that buttons ignore line-height.
    if (style->appearance() == PushButtonPart)
        style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_BUTTON, o, i.context, rect, GTK_RELIEF_NORMAL);
}

static void getComboBoxPadding(RenderStyle* style, int& left, int& top, int& right, int& bottom)
{
    // If this menu list button isn't drawn using the native theme, we
    // don't add any extra padding beyond what WebCore already uses.
    if (style->appearance() == NoControlPart)
        return;
    moz_gtk_get_widget_border(MOZ_GTK_DROPDOWN, &left, &top, &right, &bottom,
                              gtkTextDirection(style->direction()), TRUE);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return left;
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return right;
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return top;
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    int left = 0, top = 0, right = 0, bottom = 0;
    getComboBoxPadding(style, left, top, right, bottom);
    return bottom;
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // The tests check explicitly that select menu buttons ignore line height.
    style->setLineHeight(RenderStyle::initialLineHeight());

    // We cannot give a proper rendering when border radius is active, unfortunately.
    style->resetBorderRadius();
}

void RenderThemeGtk::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_DROPDOWN, o, i.context, rect);
}

bool RenderThemeGtk::paintMenuListButton(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    return paintMenuList(object, info, rect);
}

void RenderThemeGtk::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();
    style->setHeight(Length(Auto));
    style->setWhiteSpace(PRE);
    adjustMozillaStyle(this, style, MOZ_GTK_ENTRY);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintRenderObject(MOZ_GTK_ENTRY, o, i.context, rect);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintSearchFieldResultsDecoration(o, i, rect);
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

static IntRect centerRectVerticallyInParentInputElement(RenderObject* object, const IntRect& rect)
{
    IntRect centeredRect(rect);
    Node* input = object->node()->shadowAncestorNode(); // Get the renderer of <input> element.
    if (!input->renderer()->isBox()) 
        return centeredRect;

    // If possible center the y-coordinate of the rect vertically in the parent input element.
    // We also add one pixel here to ensure that the y coordinate is rounded up for box heights
    // that are even, which looks in relation to the box text.
    IntRect inputContentBox = toRenderBox(input->renderer())->absoluteContentBox();
    centeredRect.setY(inputContentBox.y() + (inputContentBox.height() - centeredRect.height() + 1) / 2);
    return centeredRect;
}

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* object, const PaintInfo& i, const IntRect& rect)
{
    static Image* searchImage = Image::loadPlatformThemeIcon(GTK_STOCK_FIND, rect.width()).releaseRef();
    IntRect centeredRect(centerRectVerticallyInParentInputElement(object, rect));
    i.context->drawImage(searchImage, ColorSpaceDeviceRGB, centeredRect);
    return false;
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* object, const PaintInfo& i, const IntRect& rect)
{
    // TODO: Brightening up the image on hover is desirable here, I believe.
    static Image* cancelImage = Image::loadPlatformThemeIcon(GTK_STOCK_CLEAR, rect.width()).releaseRef();
    IntRect centeredRect(centerRectVerticallyInParentInputElement(object, rect));
    i.context->drawImage(cancelImage, ColorSpaceDeviceRGB, centeredRect);
    return false;
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::paintSliderTrack(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderHorizontalPart || part == SliderVerticalPart);

    GtkThemeWidgetType gtkPart = MOZ_GTK_SCALE_HORIZONTAL;
    if (part == SliderVerticalPart)
        gtkPart = MOZ_GTK_SCALE_VERTICAL;

    return paintRenderObject(gtkPart, object, info.context, rect);
}

void RenderThemeGtk::adjustSliderTrackStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeGtk::paintSliderThumb(RenderObject* object, const PaintInfo& info, const IntRect& rect)
{
    ControlPart part = object->style()->appearance();
    ASSERT(part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart);

    GtkThemeWidgetType gtkPart = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
    if (part == SliderThumbVerticalPart)
        gtkPart = MOZ_GTK_SCALE_THUMB_VERTICAL;

    return paintRenderObject(gtkPart, object, info.context, rect);
}

void RenderThemeGtk::adjustSliderThumbStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

void RenderThemeGtk::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();
#if ENABLE(VIDEO)
    if (part == MediaSliderThumbPart) {
        o->style()->setWidth(Length(m_mediaSliderThumbWidth, Fixed));
        o->style()->setHeight(Length(m_mediaSliderThumbHeight, Fixed));
    } else
#endif
    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        gint width, height;
        moz_gtk_get_scalethumb_metrics(part == SliderThumbHorizontalPart ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, &width, &height);
        o->style()->setWidth(Length(width, Fixed));
        o->style()->setHeight(Length(height, Fixed));
    }
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->base[GTK_STATE_ACTIVE];
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_SELECTED];
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return gtk_widget_get_style(widget)->text[GTK_STATE_ACTIVE];
}

double RenderThemeGtk::caretBlinkInterval() const
{
    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, NULL);

    if (!shouldBlink)
        return 0;

    return time / 2000.;
}

static double getScreenDPI()
{
    // FIXME: Really this should be the widget's screen.
    GdkScreen* screen = gdk_screen_get_default();
    if (!screen)
        return 96; // Default to 96 DPI.

    float dpi = gdk_screen_get_resolution(screen);
    if (dpi <= 0)
        return 96;
    return dpi;
}

void RenderThemeGtk::systemFont(int, FontDescription& fontDescription) const
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GOwnPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), NULL);

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.get());
    if (!pangoDescription)
        return;

    fontDescription.firstFamily().setFamily(pango_font_description_get_family(pangoDescription));

    int size = pango_font_description_get_size(pangoDescription) / PANGO_SCALE;
    // If the size of the font is in points, we need to convert it to pixels.
    if (!pango_font_description_get_size_is_absolute(pangoDescription))
        size = size * (getScreenDPI() / 72.0);

    fontDescription.setSpecifiedSize(size);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
    pango_font_description_free(pangoDescription);
}

Color RenderThemeGtk::systemColor(int cssValueId) const
{
    switch (cssValueId) {
    case CSSValueButtontext:
        return Color(gtk_widget_get_style(gtkButton())->fg[GTK_STATE_NORMAL]);
    case CSSValueCaptiontext:
        return Color(gtk_widget_get_style(gtkEntry())->fg[GTK_STATE_NORMAL]);
    default:
        return RenderTheme::systemColor(cssValueId);
    }
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    // FIXME: Make sure this function doesn't get called many times for a single GTK+ style change signal.
    renderTheme->platformColorsDidChange();
}

GtkContainer* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
    m_gtkContainer = GTK_CONTAINER(gtk_fixed_new());
    g_signal_connect(m_gtkWindow, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(GTK_CONTAINER(m_gtkWindow), GTK_WIDGET(m_gtkContainer));
    gtk_widget_realize(m_gtkWindow);

    return m_gtkContainer;
}

GtkWidget* RenderThemeGtk::gtkButton() const
{
    if (m_gtkButton)
        return m_gtkButton;

    m_gtkButton = gtk_button_new();
    g_signal_connect(m_gtkButton, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkButton);
    gtk_widget_realize(m_gtkButton);

    return m_gtkButton;
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;

    m_gtkEntry = gtk_entry_new();
    g_signal_connect(m_gtkEntry, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkEntry);
    gtk_widget_realize(m_gtkEntry);

    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;

    m_gtkTreeView = gtk_tree_view_new();
    g_signal_connect(m_gtkTreeView, "style-set", G_CALLBACK(gtkStyleSetCallback), const_cast<RenderThemeGtk*>(this));
    gtk_container_add(gtkContainer(), m_gtkTreeView);
    gtk_widget_realize(m_gtkTreeView);

    return m_gtkTreeView;
}

GtkWidget* RenderThemeGtk::gtkScrollbar()
{
    return moz_gtk_get_scrollbar_widget();
}

void RenderThemeGtk::platformColorsDidChange()
{
#if ENABLE(VIDEO)
    initMediaStyling(gtk_rc_get_style(GTK_WIDGET(gtkContainer())), true);
#endif
    RenderTheme::platformColorsDidChange();
}

#if ENABLE(VIDEO)
String RenderThemeGtk::extraMediaControlsStyleSheet()
{
    return String(mediaControlsGtkUserAgentStyleSheet, sizeof(mediaControlsGtkUserAgentStyleSheet));
}

static inline bool paintMediaButton(GraphicsContext* context, const IntRect& r, Image* image, Color panelColor, int mediaIconSize)
{
    context->fillRect(FloatRect(r), panelColor, ColorSpaceDeviceRGB);
    context->drawImage(image, ColorSpaceDeviceRGB,
                       IntRect(r.x() + (r.width() - mediaIconSize) / 2,
                               r.y() + (r.height() - mediaIconSize) / 2,
                               mediaIconSize, mediaIconSize));

    return false;
}

bool RenderThemeGtk::paintMediaFullscreenButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_fullscreenButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaMuteButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    HTMLMediaElement* mediaElement = getMediaElementFromRenderObject(o);
    if (!mediaElement)
        return false;

    return paintMediaButton(paintInfo.context, r, mediaElement->muted() ? m_unmuteButton.get() : m_muteButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaPlayButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    Node* node = o->node();
    if (!node)
        return false;

    MediaControlPlayButtonElement* button = static_cast<MediaControlPlayButtonElement*>(node);
    return paintMediaButton(paintInfo.context, r, button->displayType() == MediaPlayButton ? m_playButton.get() : m_pauseButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekBackButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekBackButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSeekForwardButton(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    return paintMediaButton(paintInfo.context, r, m_seekForwardButton.get(), m_panelColor, m_mediaIconSize);
}

bool RenderThemeGtk::paintMediaSliderTrack(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    GraphicsContext* context = paintInfo.context;

    context->fillRect(FloatRect(r), m_panelColor, ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(IntRect(r.x(), r.y() + (r.height() - m_mediaSliderHeight) / 2,
                                        r.width(), m_mediaSliderHeight)), m_sliderColor, ColorSpaceDeviceRGB);

    RenderStyle* style = o->style();
    HTMLMediaElement* mediaElement = toParentMediaElement(o);

    if (!mediaElement)
        return false;

    // Draw the buffered ranges. This code is highly inspired from
    // Chrome for the gradient code.
    float mediaDuration = mediaElement->duration();
    RefPtr<TimeRanges> timeRanges = mediaElement->buffered();
    IntRect trackRect = r;
    int totalWidth = trackRect.width();

    trackRect.inflate(-style->borderLeftWidth());
    context->save();
    context->setStrokeStyle(NoStroke);

    for (unsigned index = 0; index < timeRanges->length(); ++index) {
        ExceptionCode ignoredException;
        float start = timeRanges->start(index, ignoredException);
        float end = timeRanges->end(index, ignoredException);
        int width = ((end - start) * totalWidth) / mediaDuration;
        IntRect rangeRect;
        if (!index) {
            rangeRect = trackRect;
            rangeRect.setWidth(width);
        } else {
            rangeRect.setLocation(IntPoint((start * totalWidth) / mediaDuration, trackRect.y()));
            rangeRect.setSize(IntSize(width, trackRect.height()));
        }

        // Don't bother drawing empty range.
        if (rangeRect.isEmpty())
            continue;

        IntPoint sliderTopLeft = rangeRect.location();
        IntPoint sliderTopRight = sliderTopLeft;
        sliderTopRight.move(0, rangeRect.height());

        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderTopRight);
        Color startColor = m_panelColor;
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));

        context->setFillGradient(gradient);
        context->fillRect(rangeRect);
    }

    context->restore();
    return false;
}

bool RenderThemeGtk::paintMediaSliderThumb(RenderObject* o, const PaintInfo& paintInfo, const IntRect& r)
{
    // Make the thumb nicer with rounded corners.
    paintInfo.context->fillRoundedRect(r, IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), m_sliderThumbColor, ColorSpaceDeviceRGB);
    return false;
}
#endif

#if ENABLE(PROGRESS_TAG)
double RenderThemeGtk::animationRepeatIntervalForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

double RenderThemeGtk::animationDurationForProgressBar(RenderProgress*) const
{
    // FIXME: It doesn't look like there is a good way yet to support animated
    // progress bars with the Mozilla theme drawing code.
    return 0;
}

void RenderThemeGtk::adjustProgressBarStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setBoxShadow(0);
}

bool RenderThemeGtk::paintProgressBar(RenderObject* renderObject, const PaintInfo& paintInfo, const IntRect& rect)
{
    if (!renderObject->isProgress())
        return true;

    GtkWidget* progressBarWidget = moz_gtk_get_progress_widget();
    if (!progressBarWidget)
        return true;

    if (paintRenderObject(MOZ_GTK_PROGRESSBAR, renderObject, paintInfo.context, rect))
        return true;

    IntRect chunkRect(rect);
    RenderProgress* renderProgress = toRenderProgress(renderObject);

    GtkStyle* style = gtk_widget_get_style(progressBarWidget);
    chunkRect.setHeight(chunkRect.height() - (2 * style->ythickness));
    chunkRect.setY(chunkRect.y() + style->ythickness);
    chunkRect.setWidth((chunkRect.width() - (2 * style->xthickness)) * renderProgress->position());
    if (renderObject->style()->direction() == RTL)
        chunkRect.setX(rect.x() + rect.width() - chunkRect.width() - style->xthickness);
    else
        chunkRect.setX(chunkRect.x() + style->xthickness);

    return paintRenderObject(MOZ_GTK_PROGRESS_CHUNK, renderObject, paintInfo.context, chunkRect);
}
#endif

}

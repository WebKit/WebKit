/*
 *  Copyright (C) 2010 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(VIDEO)

#include "FullscreenVideoController.h"

#include "GtkVersioning.h"
#include "MediaPlayer.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

using namespace std;
using namespace WebCore;

#define HUD_AUTO_HIDE_INTERVAL 3000 // 3 seconds
#define PROGRESS_BAR_UPDATE_INTERVAL 150 // 150ms
#define VOLUME_UP_OFFSET 0.05 // 5%
#define VOLUME_DOWN_OFFSET 0.05 // 5%

// Use symbolic icons only if we build with GTK+-3 support. They could
// be enabled for the GTK+2 build but we'd need to bump the required
// version to at least 2.22.
#if GTK_MAJOR_VERSION < 3
#define PLAY_ICON_NAME "media-playback-start"
#define PAUSE_ICON_NAME "media-playback-pause"
#define EXIT_FULLSCREEN_ICON_NAME "view-restore"
#else
#define PLAY_ICON_NAME "media-playback-start-symbolic"
#define PAUSE_ICON_NAME "media-playback-pause-symbolic"
#define EXIT_FULLSCREEN_ICON_NAME "view-restore-symbolic"
#endif

static gboolean hideHudCallback(FullscreenVideoController* controller)
{
    controller->hideHud();
    return FALSE;
}

static gboolean onFullscreenGtkMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event,  FullscreenVideoController* controller)
{
    controller->showHud(true);
    return TRUE;
}

static void onFullscreenGtkActiveNotification(GtkWidget* widget, GParamSpec* property, FullscreenVideoController* controller)
{
    if (!gtk_window_is_active(GTK_WINDOW(widget)))
        controller->hideHud();
}

static gboolean onFullscreenGtkConfigureEvent(GtkWidget* widget, GdkEventConfigure* event, FullscreenVideoController* controller)
{
    controller->gtkConfigure(event);
    return TRUE;
}

static void onFullscreenGtkDestroy(GtkWidget* widget, FullscreenVideoController* controller)
{
    controller->exitFullscreen();
}

static void togglePlayPauseActivated(GtkAction* action, FullscreenVideoController* controller)
{
    controller->togglePlay();
}

static void exitFullscreenActivated(GtkAction* action, FullscreenVideoController* controller)
{
    controller->exitOnUserRequest();
}

static gboolean progressBarUpdateCallback(FullscreenVideoController* controller)
{
    return controller->updateHudProgressBar();
}

static gboolean timeScaleButtonPressed(GtkWidget* widget, GdkEventButton* event, FullscreenVideoController* controller)
{
    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    controller->beginSeek();
    return FALSE;
}

static gboolean timeScaleButtonReleased(GtkWidget* widget, GdkEventButton* event, FullscreenVideoController* controller)
{
    controller->endSeek();
    return FALSE;
}

static void timeScaleValueChanged(GtkWidget* widget, FullscreenVideoController* controller)
{
    controller->doSeek();
}

static void volumeValueChanged(GtkScaleButton *button, gdouble value, FullscreenVideoController* controller)
{
    controller->setVolume(static_cast<float>(value));
}

void playerVolumeChangedCallback(GObject *element, GParamSpec *pspec, FullscreenVideoController* controller)
{
    controller->volumeChanged();
}

void playerMuteChangedCallback(GObject *element, GParamSpec *pspec, FullscreenVideoController* controller)
{
    controller->muteChanged();
}

FullscreenVideoController::FullscreenVideoController()
    : m_hudTimeoutId(0)
    , m_progressBarUpdateId(0)
    , m_seekLock(false)
    , m_window(0)
    , m_hudWindow(0)
{
}

FullscreenVideoController::~FullscreenVideoController()
{
    exitFullscreen();
}

void FullscreenVideoController::setMediaElement(HTMLMediaElement* mediaElement)
{
    if (mediaElement == m_mediaElement)
        return;

    m_mediaElement = mediaElement;
    if (!m_mediaElement) {
        // Can't do full-screen, just get out
        exitFullscreen();
    }
}

void FullscreenVideoController::gtkConfigure(GdkEventConfigure* event)
{
    updateHudPosition();
}

void FullscreenVideoController::showHud(bool autoHide)
{
    if (!m_hudWindow)
        return;

    if (m_hudTimeoutId) {
        g_source_remove(m_hudTimeoutId);
        m_hudTimeoutId = 0;
    }

    // Show the cursor.
    GdkWindow* window = gtk_widget_get_window(m_window);
    gdk_window_set_cursor(window, m_cursor.get());

    // Update the progress bar immediately before showing the window.
    updateHudProgressBar();
    gtk_widget_show_all(m_hudWindow);
    updateHudPosition();

    // Start periodic updates of the progress bar.
    if (!m_progressBarUpdateId)
        m_progressBarUpdateId = g_timeout_add(PROGRESS_BAR_UPDATE_INTERVAL, reinterpret_cast<GSourceFunc>(progressBarUpdateCallback), this);

    // Hide the hud in few seconds, if requested.
    if (autoHide)
        m_hudTimeoutId = g_timeout_add(HUD_AUTO_HIDE_INTERVAL, reinterpret_cast<GSourceFunc>(hideHudCallback), this);
}

void FullscreenVideoController::hideHud()
{
    if (m_hudTimeoutId) {
        g_source_remove(m_hudTimeoutId);
        m_hudTimeoutId = 0;
    }

    if (!m_hudWindow)
        return;

    // Keep the hud visible if a seek is in progress or if the volume
    // popup is visible.
    GtkWidget* volumePopup = gtk_scale_button_get_popup(GTK_SCALE_BUTTON(m_volumeButton));
    if (m_seekLock || gtk_widget_get_visible(volumePopup)) {
        showHud(true);
        return;
    }

    GdkWindow* window = gtk_widget_get_window(m_window);
    GdkCursor* cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
    gdk_window_set_cursor(window, cursor);

    gtk_widget_hide_all(m_hudWindow);

    if (m_progressBarUpdateId) {
        g_source_remove(m_progressBarUpdateId);
        m_progressBarUpdateId = 0;
    }
}

static gboolean onFullscreenGtkKeyPressEvent(GtkWidget* widget, GdkEventKey* event, FullscreenVideoController* controller)
{
    switch (event->keyval) {
    case GDK_Escape:
    case 'f':
    case 'F':
        controller->exitOnUserRequest();
        break;
    case GDK_space:
    case GDK_Return:
        controller->togglePlay();
        break;
    case GDK_Up:
        // volume up
        controller->setVolume(controller->volume() + VOLUME_UP_OFFSET);
        break;
    case GDK_Down:
        // volume down
        controller->setVolume(controller->volume() - VOLUME_DOWN_OFFSET);
        break;
    default:
        break;
    }

    return TRUE;
}


void FullscreenVideoController::enterFullscreen()
{
    if (!m_mediaElement)
        return;

    if (m_mediaElement->platformMedia().type != WebCore::PlatformMedia::GStreamerGWorldType)
        return;

    m_gstreamerGWorld = m_mediaElement->platformMedia().media.gstreamerGWorld;
    if (!m_gstreamerGWorld->enterFullscreen())
        return;

    m_window = reinterpret_cast<GtkWidget*>(m_gstreamerGWorld->platformVideoWindow()->window());

    GstElement* pipeline = m_gstreamerGWorld->pipeline();
    g_signal_connect(pipeline, "notify::volume", G_CALLBACK(playerVolumeChangedCallback), this);
    g_signal_connect(pipeline, "notify::mute", G_CALLBACK(playerMuteChangedCallback), this);

    if (!m_hudWindow)
        createHud();

    // Ensure black background.
    GdkColor color;
    gdk_color_parse("black", &color);
    gtk_widget_modify_bg(m_window, GTK_STATE_NORMAL, &color);
    gtk_widget_set_double_buffered(m_window, FALSE);

    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onFullscreenGtkKeyPressEvent), this);
    g_signal_connect(m_window, "destroy", G_CALLBACK(onFullscreenGtkDestroy), this);
    g_signal_connect(m_window, "notify::is-active", G_CALLBACK(onFullscreenGtkActiveNotification), this);

    gtk_widget_show_all(m_window);

    GdkWindow* window = gtk_widget_get_window(m_window);
    GdkCursor* cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
    m_cursor = gdk_window_get_cursor(window);
    gdk_window_set_cursor(window, cursor);
    gdk_cursor_unref(cursor);

    g_signal_connect(m_window, "motion-notify-event", G_CALLBACK(onFullscreenGtkMotionNotifyEvent), this);
    g_signal_connect(m_window, "configure-event", G_CALLBACK(onFullscreenGtkConfigureEvent), this);

    gtk_window_fullscreen(GTK_WINDOW(m_window));
    showHud(true);
}

void FullscreenVideoController::updateHudPosition()
{
    if (!m_hudWindow)
        return;

    // Get the screen rectangle.
    GdkScreen* screen = gtk_window_get_screen(GTK_WINDOW(m_window));
    GdkWindow* window = gtk_widget_get_window(m_window);
    GdkRectangle fullscreenRectangle;
    gdk_screen_get_monitor_geometry(screen, gdk_screen_get_monitor_at_window(screen, window),
                                    &fullscreenRectangle);

    // Get the popup window size.
    int hudWidth, hudHeight;
    gtk_window_get_size(GTK_WINDOW(m_hudWindow), &hudWidth, &hudHeight);

    // Resize the hud to the full width of the screen.
    gtk_window_resize(GTK_WINDOW(m_hudWindow), fullscreenRectangle.width, hudHeight);

    // Move the hud to the bottom of the screen.
    gtk_window_move(GTK_WINDOW(m_hudWindow), fullscreenRectangle.x,
                    fullscreenRectangle.height + fullscreenRectangle.y - hudHeight);
}

void FullscreenVideoController::exitOnUserRequest()
{
    m_mediaElement->exitFullscreen();
}

void FullscreenVideoController::exitFullscreen()
{
    if (!m_hudWindow)
        return;

    g_signal_handlers_disconnect_by_func(m_window, reinterpret_cast<void*>(onFullscreenGtkKeyPressEvent), this);
    g_signal_handlers_disconnect_by_func(m_window, reinterpret_cast<void*>(onFullscreenGtkDestroy), this);
    g_signal_handlers_disconnect_by_func(m_window, reinterpret_cast<void*>(onFullscreenGtkMotionNotifyEvent), this);
    g_signal_handlers_disconnect_by_func(m_window, reinterpret_cast<void*>(onFullscreenGtkConfigureEvent), this);

    GstElement* pipeline = m_mediaElement->platformMedia().media.gstreamerGWorld->pipeline();
    g_signal_handlers_disconnect_by_func(pipeline, reinterpret_cast<void*>(playerVolumeChangedCallback), this);
    g_signal_handlers_disconnect_by_func(pipeline, reinterpret_cast<void*>(playerMuteChangedCallback), this);

    if (m_hudTimeoutId) {
        g_source_remove(m_hudTimeoutId);
        m_hudTimeoutId = 0;
    }

    if (m_progressBarUpdateId) {
        g_source_remove(m_progressBarUpdateId);
        m_progressBarUpdateId = 0;
    }

    if (m_mediaElement->platformMedia().type == WebCore::PlatformMedia::GStreamerGWorldType)
        m_mediaElement->platformMedia().media.gstreamerGWorld->exitFullscreen();

    gtk_widget_hide_all(m_window);

    gtk_widget_destroy(m_hudWindow);
    m_hudWindow = 0;
}

bool FullscreenVideoController::canPlay() const
{
    return m_mediaElement && m_mediaElement->canPlay();
}

void FullscreenVideoController::play()
{
    if (m_mediaElement)
        m_mediaElement->play(m_mediaElement->processingUserGesture());

    playStateChanged();
    showHud(true);
}

void FullscreenVideoController::pause()
{
    if (m_mediaElement)
        m_mediaElement->pause(m_mediaElement->processingUserGesture());

    playStateChanged();
    showHud(false);
}

void FullscreenVideoController::playStateChanged()
{
    if (canPlay())
        g_object_set(m_playPauseAction, "tooltip", _("Play"), "icon-name", PLAY_ICON_NAME, NULL);
    else
        g_object_set(m_playPauseAction, "tooltip", _("Pause"), "icon-name", PAUSE_ICON_NAME, NULL);
}

void FullscreenVideoController::togglePlay()
{
    if (canPlay())
        play();
    else
        pause();
}

float FullscreenVideoController::volume() const
{
    return m_mediaElement ? m_mediaElement->volume() : 0;
}

bool FullscreenVideoController::muted() const
{
    return m_mediaElement ? m_mediaElement->muted() : false;
}

void FullscreenVideoController::setVolume(float volume)
{
    if (volume < 0.0 || volume > 1.0)
        return;

    if (m_mediaElement) {
        ExceptionCode ec;
        m_mediaElement->setVolume(volume, ec);
    }
}

void FullscreenVideoController::volumeChanged()
{
    g_signal_handler_block(m_volumeButton, m_volumeUpdateId);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(m_volumeButton), volume());
    g_signal_handler_unblock(m_volumeButton, m_volumeUpdateId);
}

void FullscreenVideoController::muteChanged()
{
    g_signal_handler_block(m_volumeButton, m_volumeUpdateId);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(m_volumeButton), muted() ? 0 : volume());
    g_signal_handler_unblock(m_volumeButton, m_volumeUpdateId);
}

float FullscreenVideoController::currentTime() const
{
    return m_mediaElement ? m_mediaElement->currentTime() : 0;
}

void FullscreenVideoController::setCurrentTime(float value)
{
    if (m_mediaElement) {
        ExceptionCode ec;
        m_mediaElement->setCurrentTime(value, ec);
    }
}

float FullscreenVideoController::duration() const
{
    return m_mediaElement ? m_mediaElement->duration() : 0;
}

float FullscreenVideoController::percentLoaded() const
{
    return m_mediaElement ? m_mediaElement->percentLoaded() : 0;
}

void FullscreenVideoController::beginSeek()
{
    m_seekLock = true;

    if (m_mediaElement)
        m_mediaElement->beginScrubbing();
}

void FullscreenVideoController::doSeek()
{
    if (!m_seekLock)
         return;

    setCurrentTime(gtk_range_get_value(GTK_RANGE(m_timeHScale))*duration() / 100);
}

void FullscreenVideoController::endSeek()
{
    if (m_mediaElement)
        m_mediaElement->endScrubbing();

    m_seekLock = false;
}

static String timeToString(float time)
{
    if (!isfinite(time))
        time = 0;
    int seconds = fabsf(time);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (hours) {
        if (hours > 9)
            return String::format("%s%02d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
        return String::format("%s%01d:%02d:%02d", (time < 0 ? "-" : ""), hours, minutes, seconds);
    }

    return String::format("%s%02d:%02d", (time < 0 ? "-" : ""), minutes, seconds);
}

gboolean FullscreenVideoController::updateHudProgressBar()
{
    float mediaDuration(duration());
    float mediaPosition(currentTime());

    if (!m_seekLock) {
        gdouble value = 0.0;

        if (mediaPosition && mediaDuration)
            value = (mediaPosition * 100.0) / mediaDuration;

        GtkAdjustment* adjustment = gtk_range_get_adjustment(GTK_RANGE(m_timeHScale));
        gtk_adjustment_set_value(adjustment, value);
    }

    gtk_range_set_fill_level(GTK_RANGE(m_timeHScale), percentLoaded()* 100);

    gchar* label = g_strdup_printf("%s / %s", timeToString(mediaPosition).utf8().data(),
                                   timeToString(mediaDuration).utf8().data());
    gtk_label_set_text(GTK_LABEL(m_timeLabel), label);
    g_free(label);
    return TRUE;
}

void FullscreenVideoController::createHud()
{
    m_hudWindow = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_gravity(GTK_WINDOW(m_hudWindow), GDK_GRAVITY_SOUTH_WEST);
    gtk_window_set_type_hint(GTK_WINDOW(m_hudWindow), GDK_WINDOW_TYPE_HINT_NORMAL);

    g_signal_connect(m_hudWindow, "motion-notify-event", G_CALLBACK(onFullscreenGtkMotionNotifyEvent), this);

    GtkWidget* hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(m_hudWindow), hbox);

    m_playPauseAction = gtk_action_new("play", _("Play / Pause"), _("Play or pause the media"), PAUSE_ICON_NAME);
    g_signal_connect(m_playPauseAction, "activate", G_CALLBACK(togglePlayPauseActivated), this);

    playStateChanged();

    GtkWidget* item = gtk_action_create_tool_item(m_playPauseAction);
    gtk_box_pack_start(GTK_BOX(hbox), item, FALSE, TRUE, 0);

    GtkWidget* label = gtk_label_new(_("Time:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    GtkObject* adjustment = gtk_adjustment_new(0.0, 0.0, 100.0, 0.1, 1.0, 1.0);
    m_timeHScale = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
    gtk_scale_set_draw_value(GTK_SCALE(m_timeHScale), FALSE);
    gtk_range_set_show_fill_level(GTK_RANGE(m_timeHScale), TRUE);
    gtk_range_set_update_policy(GTK_RANGE(m_timeHScale), GTK_UPDATE_CONTINUOUS);
    g_signal_connect(m_timeHScale, "button-press-event", G_CALLBACK(timeScaleButtonPressed), this);
    g_signal_connect(m_timeHScale, "button-release-event", G_CALLBACK(timeScaleButtonReleased), this);
    m_hscaleUpdateId = g_signal_connect(m_timeHScale, "value-changed", G_CALLBACK(timeScaleValueChanged), this);

    gtk_box_pack_start(GTK_BOX(hbox), m_timeHScale, TRUE, TRUE, 0);

    m_timeLabel = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), m_timeLabel, FALSE, TRUE, 0);

    // Volume button.
    m_volumeButton = gtk_volume_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), m_volumeButton, FALSE, TRUE, 0);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(m_volumeButton), volume());
    m_volumeUpdateId = g_signal_connect(m_volumeButton, "value-changed", G_CALLBACK(volumeValueChanged), this);


    m_exitFullscreenAction = gtk_action_new("exit", _("Exit Fullscreen"), _("Exit from fullscreen mode"), EXIT_FULLSCREEN_ICON_NAME);
    g_signal_connect(m_exitFullscreenAction, "activate", G_CALLBACK(exitFullscreenActivated), this);
    g_object_set(m_exitFullscreenAction, "icon-name", EXIT_FULLSCREEN_ICON_NAME, NULL);
    item = gtk_action_create_tool_item(m_exitFullscreenAction);
    gtk_box_pack_start(GTK_BOX(hbox), item, FALSE, TRUE, 0);


    m_progressBarUpdateId = g_timeout_add(PROGRESS_BAR_UPDATE_INTERVAL, reinterpret_cast<GSourceFunc>(progressBarUpdateCallback), this);
}

#endif

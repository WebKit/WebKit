/*
 * Copyright (C) 2012 Zan Dobersek <zandobersek@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "Gamepads.h"

#if ENABLE(GAMEPAD_DEPRECATED)

#include "GamepadDeviceLinux.h"
#include "GamepadList.h"
#include "Logging.h"
#include <gio/gunixinputstream.h>
#include <gudev/gudev.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class GamepadDeviceGlib : public GamepadDeviceLinux {
public:
    explicit GamepadDeviceGlib(String deviceFile);
    ~GamepadDeviceGlib();

private:
    static gboolean readCallback(GObject* pollableStream, gpointer data);
    GRefPtr<GInputStream> m_inputStream;
    GRefPtr<GSource> m_source;
};

GamepadDeviceGlib::GamepadDeviceGlib(String deviceFile)
    : GamepadDeviceLinux(deviceFile)
{
    if (m_fileDescriptor == -1)
        return;

    m_inputStream = adoptGRef(g_unix_input_stream_new(m_fileDescriptor, FALSE));
    m_source = adoptGRef(g_pollable_input_stream_create_source(G_POLLABLE_INPUT_STREAM(m_inputStream.get()), 0));
    g_source_set_callback(m_source.get(), reinterpret_cast<GSourceFunc>(readCallback), this, 0);
    g_source_attach(m_source.get(), 0);
}

GamepadDeviceGlib::~GamepadDeviceGlib()
{
    if (m_source)
        g_source_destroy(m_source.get());
}

gboolean GamepadDeviceGlib::readCallback(GObject* pollableStream, gpointer data)
{
    GamepadDeviceGlib* gamepadDevice = reinterpret_cast<GamepadDeviceGlib*>(data);
    GUniqueOutPtr<GError> error;
    struct js_event event;

    gssize len = g_pollable_input_stream_read_nonblocking(G_POLLABLE_INPUT_STREAM(pollableStream),
        &event, sizeof(event), 0, &error.outPtr());

    // FIXME: Properly log the error.
    // In the case of G_IO_ERROR_WOULD_BLOCK error return TRUE to wait until
    // the source becomes readable again and FALSE otherwise.
    if (error)
        return g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);

    ASSERT_UNUSED(len, len == sizeof(event));
    gamepadDevice->updateForEvent(event);
    return TRUE;
}

class GamepadsGlib {
public:
    explicit GamepadsGlib(unsigned length);

    void registerDevice(String deviceFile);
    void unregisterDevice(String deviceFile);

    void updateGamepadList(GamepadList*);

private:
    ~GamepadsGlib();
    static void onUEventCallback(GUdevClient*, gchar* action, GUdevDevice*, gpointer data);
    static gboolean isGamepadDevice(GUdevDevice*);

    Vector<std::unique_ptr<GamepadDeviceGlib> > m_slots;
    HashMap<String, GamepadDeviceGlib*> m_deviceMap;

    GRefPtr<GUdevClient> m_gudevClient;
};

GamepadsGlib::GamepadsGlib(unsigned length)
    : m_slots(length)
{
    static const char* subsystems[] = { "input", 0 };
    m_gudevClient = adoptGRef(g_udev_client_new(subsystems));
    g_signal_connect(m_gudevClient.get(), "uevent", G_CALLBACK(onUEventCallback), this);

    GUniquePtr<GList> devicesList(g_udev_client_query_by_subsystem(m_gudevClient.get(), subsystems[0]));
    for (GList* listItem = devicesList.get(); listItem; listItem = g_list_next(listItem)) {
        GUdevDevice* device = G_UDEV_DEVICE(listItem->data);
        String deviceFile = String::fromUTF8(g_udev_device_get_device_file(device));
        if (isGamepadDevice(device))
            registerDevice(deviceFile);
        g_object_unref(device);
    }
}

GamepadsGlib::~GamepadsGlib()
{
    g_signal_handlers_disconnect_matched(m_gudevClient.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
}

void GamepadsGlib::registerDevice(String deviceFile)
{
    LOG(Gamepad, "GamepadsGlib::registerDevice %s", deviceFile.ascii().data());
    ASSERT(!m_deviceMap.contains(deviceFile));

    for (unsigned index = 0; index < m_slots.size(); index++) {
        if (!m_slots[index]) {
            m_slots[index] = std::make_unique<GamepadDeviceGlib>(deviceFile);
            m_deviceMap.add(deviceFile, m_slots[index].get());
            break;
        }
    }
}

void GamepadsGlib::unregisterDevice(String deviceFile)
{
    LOG(Gamepad, "GamepadsGlib::unregisterDevice %s", deviceFile.ascii().data());
    ASSERT(m_deviceMap.contains(deviceFile));

    GamepadDeviceGlib* gamepadDevice = m_deviceMap.take(deviceFile);
    size_t index = m_slots.find(gamepadDevice);
    ASSERT(index != notFound);

    m_slots[index] = nullptr;
}

void GamepadsGlib::updateGamepadList(GamepadList* into)
{
    ASSERT(m_slots.size() == into->length());

    for (unsigned i = 0; i < m_slots.size(); i++) {
        if (m_slots[i].get() && m_slots[i]->connected()) {
            GamepadDeviceGlib* gamepadDevice = m_slots[i].get();
            RefPtr<Gamepad> gamepad = into->item(i);
            if (!gamepad)
                gamepad = Gamepad::create();

            gamepad->index(i);
            gamepad->id(gamepadDevice->id());
            gamepad->timestamp(gamepadDevice->timestamp());
            gamepad->axes(gamepadDevice->axesCount(), gamepadDevice->axesData());
            gamepad->buttons(gamepadDevice->buttonsCount(), gamepadDevice->buttonsData());

            into->set(i, gamepad);
        } else
            into->set(i, 0);
    }
}

void GamepadsGlib::onUEventCallback(GUdevClient*, gchar* action, GUdevDevice* device, gpointer data)
{
    if (!isGamepadDevice(device))
        return;

    GamepadsGlib* gamepadsGlib = reinterpret_cast<GamepadsGlib*>(data);
    String deviceFile = String::fromUTF8(g_udev_device_get_device_file(device));

    if (!g_strcmp0(action, "add"))
        gamepadsGlib->registerDevice(deviceFile);
    else if (!g_strcmp0(action, "remove"))
        gamepadsGlib->unregisterDevice(deviceFile);
}

gboolean GamepadsGlib::isGamepadDevice(GUdevDevice* device)
{
    const gchar* deviceFile = g_udev_device_get_device_file(device);
    const gchar* sysfsPath = g_udev_device_get_sysfs_path(device);
    if (!deviceFile || !sysfsPath)
        return FALSE;

    if (!g_udev_device_has_property(device, "ID_INPUT") || !g_udev_device_has_property(device, "ID_INPUT_JOYSTICK"))
        return FALSE;

    return g_str_has_prefix(deviceFile, "/dev/input/js");
}

void sampleGamepads(GamepadList* into)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(GamepadsGlib, gamepadsGlib, (into->length()));
    gamepadsGlib.updateGamepadList(into);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD_DEPRECATED)

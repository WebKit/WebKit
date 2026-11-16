/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEProcessManager.h"

#if OS(ANDROID)

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEProcessLaunchOptions:
 *
 * Boxed type describing an auxiliary process WebKit wants to launch, passed to
 * [vfunc@ProcessManager.launch].
 */
struct _WPEProcessLaunchOptions {
    WPEProcessType processType;
    guint64 processID;
    int ipcSocketFD;
};

/**
 * wpe_process_launch_options_copy:
 * @options: a #WPEProcessLaunchOptions
 *
 * Make a copy of @options.
 *
 * Returns: (transfer full): a copy of @options.
 */
WPEProcessLaunchOptions* wpe_process_launch_options_copy(WPEProcessLaunchOptions* options)
{
    g_return_val_if_fail(options, nullptr);

    auto* copy = static_cast<WPEProcessLaunchOptions*>(fastZeroedMalloc(sizeof(WPEProcessLaunchOptions)));
    *copy = *options;
    return copy;
}

/**
 * wpe_process_launch_options_free:
 * @options: a #WPEProcessLaunchOptions
 *
 * Free @options.
 */
void wpe_process_launch_options_free(WPEProcessLaunchOptions* options)
{
    g_return_if_fail(options);

    fastFree(options);
}

G_DEFINE_BOXED_TYPE(WPEProcessLaunchOptions, wpe_process_launch_options, wpe_process_launch_options_copy, wpe_process_launch_options_free)

/**
 * wpe_process_launch_options_new:
 * @process_type: the #WPEProcessType to launch.
 * @process_id: the process identifier assigned to the new process.
 * @ipc_socket_fd: the file descriptor of the IPC socket the launched process
 *   must use to communicate with the UI process.
 *
 * Create a new #WPEProcessLaunchOptions.
 *
 * @process_id is an identifier that WebKit assigns to the new auxiliary
 * process and it adopts as its own identity. Note that this identifier
 * *does not* correspond to any other kind of identifier that the operating
 * system may use for processes, i.e. it is not a `pid_t`.
 *
 * The auxiliary process must be started so that it receives @process_id and
 * @ipc_socket_fd as its first two command line arguments, in that order (the
 * process identifier first, then the IPC socket file descriptor); that is how
 * WebKit's auxiliary process entry point expects to receive them.
 *
 * Returns: (transfer full): a new #WPEProcessLaunchOptions.
 */
WPEProcessLaunchOptions* wpe_process_launch_options_new(WPEProcessType processType, guint64 processID, int ipcSocketFD)
{
    WPEProcessLaunchOptions* options = static_cast<WPEProcessLaunchOptions*>(fastZeroedMalloc(sizeof(WPEProcessLaunchOptions)));
    options->processType = processType;
    options->processID = processID;
    options->ipcSocketFD = ipcSocketFD;
    return options;
}

/**
 * wpe_process_launch_options_get_process_type:
 * @options: a #WPEProcessLaunchOptions
 *
 * Get the #WPEProcessType of the process to launch.
 *
 * Returns: the #WPEProcessType.
 */
WPEProcessType wpe_process_launch_options_get_process_type(WPEProcessLaunchOptions* options)
{
    g_return_val_if_fail(options, WPE_PROCESS_TYPE_WEB);

    return options->processType;
}

/**
 * wpe_process_launch_options_get_process_id:
 * @options: a #WPEProcessLaunchOptions
 *
 * Get the identifier the UI process assigned to the process.
 *
 * This is the process identifier that WebKit assigns to the new process. See
 * [id@wpe_process_launch_options_new] for details.
 *
 * Returns: the process identifier.
 */
guint64 wpe_process_launch_options_get_process_id(WPEProcessLaunchOptions* options)
{
    g_return_val_if_fail(options, 0);

    return options->processID;
}

/**
 * wpe_process_launch_options_get_ipc_socket_fd:
 * @options: a #WPEProcessLaunchOptions
 *
 * Get the file descriptor of the IPC socket the launched process must use to
 * communicate with the UI process.
 *
 * Returns: the IPC socket file descriptor.
 */
int wpe_process_launch_options_get_ipc_socket_fd(WPEProcessLaunchOptions* options)
{
    g_return_val_if_fail(options, -1);

    return options->ipcSocketFD;
}

/**
 * WPEProcessManager:
 *
 * Allows the embedder to launch and terminate WebKit's auxiliary processes on
 * Android.
 *
 * WebKit normally spawns its auxiliary processes as standalone executables using
 * the `g_spawn_*()` family of functions from GLib. That does not work on Android,
 * where each auxiliary process must instead be created as a [bound
 * service](https://developer.android.com/develop/background-work/services/bound-services).
 * On Android the embedder subclasses #WPEProcessManager and registers an instance
 * as the default with [id@wpe_process_manager_set_default].
 */

struct _WPEProcessManagerPrivate {
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEProcessManager, wpe_process_manager, G_TYPE_OBJECT)

G_DEFINE_QUARK(wpe-process-manager-error-quark, wpe_process_manager_error)

static void wpe_process_manager_class_init(WPEProcessManagerClass*)
{
}

/**
 * wpe_process_manager_launch:
 * @manager: a #WPEProcessManager.
 * @options: the #WPEProcessLaunchOptions describing the process to launch.
 * @error: return location for error or %NULL to ignore.
 *
 * Launch an auxiliary process described by @options.
 *
 * See [vfunc@ProcessManager.launch] for details about the returned identifier.
 *
 * Returns: an identifier for the launched process, or 0 in case of error.
 */
guint64 wpe_process_manager_launch(WPEProcessManager* manager, WPEProcessLaunchOptions* options, GError** error)
{
    g_return_val_if_fail(WPE_IS_PROCESS_MANAGER(manager), 0);

    return WPE_PROCESS_MANAGER_GET_CLASS(manager)->launch(manager, options, error);
}

/**
 * wpe_process_manager_terminate:
 * @manager: a #WPEProcessManager.
 * @process_id: the identifier of the process to terminate, as returned by
 *   wpe_process_manager_launch().
 *
 * Terminates a process previously launched through @manager.
 */
void wpe_process_manager_terminate(WPEProcessManager* manager, guint64 processID)
{
    g_return_if_fail(WPE_IS_PROCESS_MANAGER(manager));

    WPE_PROCESS_MANAGER_GET_CLASS(manager)->terminate(manager, processID);
}

static GRefPtr<WPEProcessManager> s_defaultProcessManager;

/**
 * wpe_process_manager_get_default:
 *
 * Get the default #WPEProcessManager, or %NULL if none has been set.
 *
 * The default process manager is the one WebKit uses to launch and terminate
 * its auxiliary processes. See [id@wpe_process_manager_set_default].
 *
 * Returns: (transfer none) (nullable): the default #WPEProcessManager, or %NULL
 */
WPEProcessManager* wpe_process_manager_get_default(void)
{
    return s_defaultProcessManager.get();
}

/**
 * wpe_process_manager_set_default:
 * @manager: (nullable): a #WPEProcessManager, or %NULL to unset
 *
 * Set the default #WPEProcessManager used by WebKit to launch and terminate its
 * auxiliary processes.
 *
 * This is meant to be set once at startup by the embedder on Android, where the
 * auxiliary processes cannot be spawned as standalone executables.
 */
void wpe_process_manager_set_default(WPEProcessManager* manager)
{
    g_return_if_fail(!manager || WPE_IS_PROCESS_MANAGER(manager));
    s_defaultProcessManager = manager;
}

#endif // OS(ANDROID)

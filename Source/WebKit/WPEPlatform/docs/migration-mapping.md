Title: Migration mapping table
Slug: migration-mapping

This page is a symbol-by-symbol reference for migrating code written
against [libwpe](https://github.com/WebKit/libwpe) and
[WPEBackend-fdo](https://github.com/Igalia/WPEBackend-fdo) to
WPEPlatform. For prose, before/after code, and discussion of the
conceptual shifts, see [Migrating from libwpe](migrating-from-libwpe.html).

The table records what each old symbol *does*, the closest WPEPlatform
equivalent, and — where there isn't one — why. Three classes of entry:

- **Direct.** A symbol that maps to a single WPEPlatform function or class with comparable semantics.
- **Reshape.** A symbol whose role is preserved but framed differently (e.g. an opaque C struct + vtable replaced by a GObject + virtual methods).
- **Removed.** A symbol whose role is no longer the platform layer's responsibility (e.g. handled internally by WPE WebKit, or dropped entirely).

---

## libwpe

### `wpe/view-backend.h`

The single biggest concept change. `wpe_view_backend` and its
companion client/handler interfaces are gone; their responsibilities
are split across [class@View] and [class@Toplevel].

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_view_backend` | [class@View] + [class@Toplevel] | Reshape | The single opaque struct becomes two GObject classes. View owns rendering surface; toplevel owns window-level state. |
| `wpe_view_backend_create()` | [ctor@View.new] | Reshape | View constructor now takes a [class@Display] argument. |
| `wpe_view_backend_create_with_backend_interface(iface, data)` | Subclass [class@View] / [class@Display] | Reshape | The C-vtable extension model is replaced by GObject subclassing. |
| `wpe_view_backend_destroy()` | `g_object_unref()` | Direct | Standard GObject lifecycle. |
| `struct wpe_view_backend_interface` | [class@View] vfuncs + [class@Display] vfuncs | Reshape | Vtable split across `WPEViewClass` and `WPEDisplayClass`. |
| `wpe_view_backend_set_backend_client(...)` | View signals (`closed`, `resized`, `buffers-changed`, `buffer-rendered`, `buffer-released`, `toplevel-state-changed`, `preferred-buffer-formats-changed`) | Reshape | Per-event signal connections replace a single client struct. |
| `wpe_view_backend_client.set_size` | [signal@View::resized] | Direct | |
| `wpe_view_backend_client.frame_displayed` | [signal@View::buffer-rendered] | Direct | |
| `wpe_view_backend_client.activity_state_changed` | [signal@View::toplevel-state-changed] + view focus/visible state | Reshape | Old `wpe_view_activity_state` bitfield (visible / focused / in_window) split across [flags@ToplevelState], [method@View.get_has_focus], [method@View.get_visible]. |
| `wpe_view_backend_client.get_accessible` | [method@View.get_accessible] / [iface@ViewAccessible] | Reshape | Now a GObject interface; the accessible is bound by name via [method@ViewAccessible.bind]. |
| `wpe_view_backend_client.set_device_scale_factor` | [method@View.get_scale] / [method@Screen.set_scale] | Reshape | Scale is now driven by the screen the view is on. |
| `wpe_view_backend_client.target_refresh_rate_changed` | [method@Screen.set_refresh_rate] | Reshape | Refresh rate is a screen attribute, not a view attribute. |
| `wpe_view_backend_initialize()` | Implicit (handled by view/display lifecycle) | Removed | No separate initialize step. |
| `wpe_view_backend_get_renderer_host_fd()` | — | Removed | The renderer-host fd plumbing is gone. WPE WebKit handles UI↔Web-process IPC internally. |
| `wpe_view_backend_set_input_client(...)` | [signal@View::event] | Reshape | Single signal replaces per-input-type callback struct. |
| `wpe_view_backend_input_client.handle_keyboard_event` | [signal@View::event] with `WPE_EVENT_KEYBOARD_KEY_DOWN`/`KEYBOARD_KEY_UP` | Direct | |
| `wpe_view_backend_input_client.handle_pointer_event` | [signal@View::event] with `WPE_EVENT_POINTER_DOWN`/`UP`/`MOVE`/`ENTER`/`LEAVE` | Direct | |
| `wpe_view_backend_input_client.handle_axis_event` | [signal@View::event] with `WPE_EVENT_SCROLL` | Direct | |
| `wpe_view_backend_input_client.handle_touch_event` | [signal@View::event] with `WPE_EVENT_TOUCH_DOWN`/`UP`/`MOVE`/`CANCEL` | Direct | |
| `wpe_view_backend_input_client.handle_pointer_lock_event` | [method@View.lock_pointer] / [method@View.unlock_pointer] | Reshape | Lock/unlock are direct view operations. |
| `wpe_view_backend_dispatch_set_size(w, h)` | [method@View.resized] | Direct | (Notification form, called by platform implementations.) |
| `wpe_view_backend_dispatch_frame_displayed()` | [method@View.buffer_rendered] | Direct | |
| `wpe_view_activity_state_visible` (set via `add_activity_state`) | [method@View.set_visible] to assert the state; [method@View.get_mapped] is what WebKit actually reads | Reshape | "Visible" in libwpe corresponds to WPEPlatform's *mapped* state (visible AND not hidden by toplevel state). `wpe_view_get_visible` alone can be `TRUE` while `get_mapped` is `FALSE` (e.g. minimized). Observe with `notify::mapped`. |
| `wpe_view_activity_state_focused` | [method@View.focus_in] / [method@View.focus_out] to set; [method@View.get_has_focus] to read; `notify::has-focus` to observe | Direct | |
| `wpe_view_activity_state_in_window` | A view is "in window" when it has a toplevel attached: [method@View.set_toplevel] (set), `wpe_view_get_toplevel() != NULL` (read), `notify::toplevel` (observe) | Reshape | By default views are created with a toplevel automatically (see [const@SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL]). |
| `wpe_view_backend_add_activity_state()` / `_remove_activity_state()` / `_get_activity_state()` bulk APIs | The three per-bit accessors above | Reshape | The bitfield is decomposed; there is no single getter or setter. |
| WebKit's `ActivityState::WindowIsActive` (drawn from the active-toplevel signal of libwpe) | `WPE_TOPLEVEL_STATE_ACTIVE` from [method@Toplevel.get_state]; observed per-view via [signal@View::toplevel-state-changed] | Reshape | Active-toplevel state is now a [flags@ToplevelState] bit. |
| `wpe_view_backend_dispatch_get_accessible()` | [method@View.get_accessible] | Direct | |
| `wpe_view_backend_dispatch_set_device_scale_factor()` | Scale is a property of [class@Screen]: set with [method@Screen.set_scale] | Reshape | The scale propagates from the screen to the [class@Toplevel] (which reads [method@Screen.get_scale]) and then to the [class@View] ([method@View.get_scale] returns the toplevel's scale). Embedders that previously set the factor on the view backend now set it on the screen instead. |
| `wpe_view_backend_get_target_refresh_rate()` / `_set_target_refresh_rate()` | [method@Screen.get_refresh_rate] / [method@Screen.set_refresh_rate] | Reshape | Refresh rate is now stored on the screen, not the view backend. WebKit reads it from `wpe_view_get_screen()` → `wpe_screen_get_refresh_rate()`. |
| `wpe_view_backend_dispatch_keyboard_event()` | [method@View.event] with a keyboard [struct@Event] | Reshape | Construct event with [ctor@Event.keyboard_new], then `wpe_view_event()`. |
| `wpe_view_backend_dispatch_pointer_event()` | [method@View.event] with a pointer [struct@Event] | Reshape | [ctor@Event.pointer_button_new] / [ctor@Event.pointer_move_new]. |
| `wpe_view_backend_dispatch_axis_event()` | [method@View.event] with `wpe_event_scroll_new()` | Reshape | |
| `wpe_view_backend_dispatch_touch_event()` | [method@View.event] with `wpe_event_touch_new()` | Reshape | |
| `wpe_view_backend_dispatch_pointer_lock_event()` | — | Removed | The pointer-lock event struct is gone; lock/unlock are state operations, not events. |
| `wpe_view_backend_set_fullscreen_client()` / `_set_fullscreen_handler()` / `wpe_view_backend_fullscreen_client` / `wpe_view_backend_fullscreen_handler` | [class@Toplevel] state API + [signal@View::toplevel-state-changed] | Reshape | Fullscreen is a [flags@ToplevelState] bit; control via [method@Toplevel.fullscreen] / `_unfullscreen`. |
| `wpe_view_backend_platform_set_fullscreen()` | [method@Toplevel.fullscreen] / `_unfullscreen()` | Reshape | |
| `wpe_view_backend_dispatch_did_enter_fullscreen()` / `_did_exit_fullscreen()` / `_request_enter_fullscreen()` / `_request_exit_fullscreen()` | [signal@View::toplevel-state-changed] | Reshape | Single signal observes all state transitions. |
| `wpe_view_backend_set_pointer_lock_handler()` / `wpe_view_backend_pointer_lock_handler` | [vfunc@View.lock_pointer] / [vfunc@View.unlock_pointer] | Reshape | Override on a [class@View] subclass instead of registering a callback. |
| `wpe_view_backend_request_pointer_lock()` / `_request_pointer_unlock()` | [method@View.lock_pointer] / `_unlock_pointer()` | Direct | |

### `wpe/input.h`

The event-struct family becomes a single refcounted [struct@Event]
with typed constructors and accessors.

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_input_keyboard_event` | [struct@Event] + `WPE_EVENT_KEYBOARD_KEY_DOWN`/`KEYBOARD_KEY_UP` | Reshape | |
| `struct wpe_input_pointer_event` | [struct@Event] + `WPE_EVENT_POINTER_DOWN`/`UP`/`MOVE`/`ENTER`/`LEAVE` | Reshape | Pointer-button and pointer-move are separate constructors in WPEPlatform. |
| `enum wpe_input_pointer_event_type` (`null`/`motion`/`button`) | [enum@EventType] | Reshape | Pointer events are split into more granular event types (down/up/move/enter/leave). |
| `struct wpe_input_pointer_lock_event` | — | Removed | Pointer-lock has no event struct; it's a state transition (see [method@View.lock_pointer]). |
| `struct wpe_input_axis_event` | [struct@Event] + `WPE_EVENT_SCROLL` | Reshape | |
| `struct wpe_input_axis_2d_event` | [struct@Event] + `WPE_EVENT_SCROLL` (delta_x and delta_y both present) | Reshape | |
| `enum wpe_input_axis_event_type` | — | Removed | Subsumed by [struct@Event] type. |
| `struct wpe_input_touch_event` | [struct@Event] + `WPE_EVENT_TOUCH_DOWN`/`UP`/`MOVE`/`CANCEL` | Reshape | Multi-touch is now a sequence of single-point events keyed by sequence id. |
| `struct wpe_input_touch_event_raw` | — | Removed | No longer needed: each touch event in WPEPlatform represents one touchpoint identified by `wpe_event_touch_get_sequence_id()`. |
| `enum wpe_input_touch_event_type` | [enum@EventType] | Reshape | |
| `enum wpe_input_modifier` | [flags@Modifiers] | Direct | `keyboard_modifier_control/shift/alt/meta` → `WPE_MODIFIER_KEYBOARD_CONTROL/SHIFT/ALT/META`. Pointer button modifiers preserved. Also adds `WPE_MODIFIER_KEYBOARD_CAPS_LOCK`. |
| `wpe_key_code_to_unicode(uint32_t)` | [func@keyval_to_unicode] | Direct | Note the rename: `keyval`, not `key_code`. |
| `wpe_unicode_to_key_code(uint32_t)` | [func@unicode_to_keyval] | Direct | Same rename. |

### `wpe/input-xkb.h`

All wrapped in [class@KeymapXKB], a [class@Keymap] subclass.

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_input_xkb_context` | [class@KeymapXKB] | Reshape | GObject; per-display rather than a global singleton. |
| `wpe_input_xkb_context_get_default()` | [method@Display.get_keymap] | Reshape | Keymap is now obtained from the display. |
| `wpe_input_xkb_context_get_context()` | — | Removed | xkb_context is internal. |
| `wpe_input_xkb_context_get_keymap()` | [method@KeymapXKB.get_xkb_keymap] | Direct | |
| `wpe_input_xkb_context_set_keymap()` | [method@KeymapXKB.update] | Reshape | Update from fd/size pair; takes raw keymap descriptor format/fd/size. |
| `wpe_input_xkb_context_get_state()` | [method@KeymapXKB.get_xkb_state] | Direct | |
| `wpe_input_xkb_context_get_compose_table()` / `_set_compose_table()` / `_get_compose_state()` | — | Removed | xkb_compose handling is internal to the keymap implementation. |
| `wpe_input_xkb_context_get_modifiers(...)` | [method@Keymap.get_modifiers] | Reshape | Generic on [class@Keymap], not XKB-specific. |
| `wpe_input_xkb_context_get_key_code(...)` | [method@Keymap.translate_keyboard_state] | Reshape | Different signature but covers the same functionality. |
| `wpe_input_xkb_context_get_entries_for_key_code(...)` | [method@Keymap.get_entries_for_keyval] | Reshape | Note the direction reversal: WPEPlatform looks up keycodes given a keyval. |
| `struct wpe_input_xkb_keymap_entry` (hardware_key_code/layout/level) | [struct@KeymapEntry] (keycode/group/level) | Direct | Renamed but structurally equivalent. |

### `wpe/keysyms.h`

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `WPE_KEY_*` constants | `WPE_KEY_*` constants in `<wpe/WPEKeysyms.h>` | Direct | The constant names are preserved; both files derive from the X11 keysym list and cover the same ~2280 names (a couple of stragglers differ, e.g. `WPE_KEY_WindowClearGrab` → `WPE_KEY_ClearGrab`). |

### `wpe/pasteboard.h`

Replaced by [class@Clipboard], which is more capable (multi-format,
GBytes-based, change tracking) than the old singleton.

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_pasteboard` | [class@Clipboard] | Reshape | GObject, obtained from the display via [method@Display.get_clipboard]. |
| `wpe_pasteboard_get_singleton()` | [method@Display.get_clipboard] | Reshape | Per-display, not global. |
| `struct wpe_pasteboard_interface` | [class@Clipboard] vfuncs (`read`, `changed`) | Reshape | |
| `wpe_pasteboard_get_types()` | [method@Clipboard.get_formats] | Direct | Returns formats (MIME types). |
| `wpe_pasteboard_get_string()` | [method@Clipboard.read_text] | Direct | |
| `wpe_pasteboard_write()` | [method@Clipboard.set_content] + [struct@ClipboardContent] builder | Reshape | Content is built using [ctor@ClipboardContent.new] + [method@ClipboardContent.set_text] / `_set_bytes()`. |
| `struct wpe_pasteboard_string` / `_string_vector` / `_string_pair` / `_string_map` | — | Removed | Replaced by GBytes + string arrays. |
| `wpe_pasteboard_string_initialize()` / `_string_free()` / `_string_vector_free()` | — | Removed | No equivalent needed: standard GLib types do this work. |

### `wpe/gamepad.h`

Maps to [class@Gamepad] / [class@GamepadManager], both GObject.

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_gamepad_provider` | [class@GamepadManager] | Reshape | |
| `struct wpe_gamepad` | [class@Gamepad] | Reshape | |
| `enum wpe_gamepad_axis` (4 axes) | [enum@GamepadAxis] | Reshape | Member names changed: `LEFT_STICK_X` → `LEFT_X`, etc. |
| `enum wpe_gamepad_button` (17 buttons) | [enum@GamepadButton] | Reshape | Member names changed: `BOTTOM`/`RIGHT`/`LEFT`/`TOP` → `RIGHT_CLUSTER_BOTTOM/RIGHT/LEFT/TOP`, etc. |
| `wpe_gamepad_provider_create()` / `_destroy()` | [method@Display.create_gamepad_manager] / `g_object_unref()` | Reshape | Manager comes from the display, not a global factory. |
| `wpe_gamepad_provider_set_client()` | [signal@GamepadManager::device-added] / `device-removed` | Reshape | |
| `wpe_gamepad_provider_start()` / `_stop()` | — | Removed | Lifecycle is implicit; the manager is alive as long as the display is. |
| `wpe_gamepad_provider_get_backend()` | — | Removed | No equivalent; per-implementation private data is accessed via the GObject subclass directly. |
| `wpe_gamepad_provider_get_view_backend()` | — | Removed | Gamepads are not associated with a specific view in WPEPlatform. |
| `wpe_gamepad_provider_dispatch_gamepad_connected()` / `_disconnected()` | [method@GamepadManager.add_device] / `_remove_device()` | Direct | |
| `wpe_gamepad_create()` / `_destroy()` | Subclass [class@Gamepad] in the platform implementation | Reshape | |
| `wpe_gamepad_set_client()` | [signal@Gamepad::button-event] / `axis-event` | Reshape | |
| `wpe_gamepad_get_id()` | [method@Gamepad.get_name] | Direct | Renamed. |
| `wpe_gamepad_dispatch_button_changed()` / `_analog_button_changed()` | [method@Gamepad.button_event] | Reshape | Single API for both press/release events. |
| `wpe_gamepad_dispatch_axis_changed()` | [method@Gamepad.axis_event] | Direct | |
| `wpe_gamepad_set_handler()` | Subclassing | Reshape | The C-callback handler is replaced by GObject subclassing of both [class@Gamepad] and [class@GamepadManager]. |
| `struct wpe_gamepad_provider_interface` | [class@GamepadManager] subclass (no vfuncs) | Reshape | The manager has no virtual methods to override; device enumeration drives [method@GamepadManager.add_device] / `_remove_device`. |
| `struct wpe_gamepad_interface` | [class@Gamepad] virtual methods (`start_input_monitor`, `stop_input_monitor`, `has_rumble`, `rumble`) | Reshape | New WPEPlatform additions: rumble support. |

### `wpe/loader.h`

The `dlopen-the-.so-you-passed` model is replaced by GIO extension
points.

<!-- FIXME: link backend-model.html ("See Backend model.") from the loader-interface row once that page lands -->

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `wpe_loader_init("libfoo.so")` | `WPE_DISPLAY=<name>` env var, **or** direct instantiation (e.g. `wpe_display_wayland_new()`) | Reshape | The "select a backend by .so path" mechanism is gone. |
| `struct wpe_loader_interface` (`_wpe_loader_interface` symbol) | GIO extension point `"wpe-platform-display"` (`WPE_DISPLAY_EXTENSION_POINT_NAME`) | Reshape | A module is now a GIO type module that registers itself with this extension point. |
| `wpe_loader_get_loaded_implementation_library_name()` | — | Removed | No direct equivalent; introspect the [class@Display] instance instead (`G_OBJECT_TYPE_NAME()`). |

### `wpe/process.h` (libwpe ≥ 1.14)

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `enum wpe_process_type` | — | Removed | |
| `struct wpe_process_provider` | — | Removed | |
| `struct wpe_process_provider_interface` | — | Removed | |
| `wpe_process_provider_create()` / `_destroy()` / `_register_interface()` | — | Removed | |
| `wpe_process_launch()` / `wpe_process_terminate()` | — | Removed | Child-process launch is internal to WPE WebKit again. Platforms no longer participate. |

### `wpe/renderer-host.h`

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_renderer_host_interface` | — | Removed | |
| `wpe_renderer_host_create_client()` | — | Removed | UI↔Web-process IPC bootstrapping is internal. |

### `wpe/renderer-backend-egl.h` (via `wpe/wpe-egl.h`)

The EGL renderer-target abstraction is replaced by buffer sharing.

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_renderer_backend_egl` / `_target` / `_offscreen_target` | — | Removed | Replaced by [class@Buffer] and [vfunc@View.render_buffer]. |
| `struct wpe_renderer_backend_egl_interface` | — | Removed | |
| `struct wpe_renderer_backend_egl_target_interface` | — | Removed | |
| `wpe_renderer_backend_egl_create()` / `_destroy()` | — | Removed | |
| `wpe_renderer_backend_egl_get_native_display()` | [method@Display.get_egl_display] | Reshape | **Type semantics differ.** libwpe returned `EGLNativeDisplayType` (e.g. an X11 `Display*` or `wl_display*`); WPEPlatform returns the already-initialised `EGLDisplay`. Embedders migrating code that called `eglGetDisplay()` on the libwpe-returned value should drop that step. |
| `wpe_renderer_backend_egl_get_platform()` | — | Removed | Not needed (handled internally). |
| `wpe_renderer_backend_egl_target_*` (create, set_client, initialize, get_native_window, resize, frame_will_render, frame_rendered, deinitialize) | [class@View] + [class@Buffer] flow | Reshape | The whole concept of an EGL target wrapping a native window is gone; instead, the view receives [class@Buffer] objects via [vfunc@View.render_buffer]. |
| `wpe_renderer_backend_egl_target_dispatch_frame_complete()` | [method@View.buffer_rendered] / [signal@View::buffer-rendered] | Reshape | |
| `wpe_renderer_backend_egl_offscreen_target_*` | — | Removed | |

### `wpe/export.h`, `wpe/version.h`, `wpe/libwpe-version.h`, `wpe/version-deprecated.h`

| libwpe symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `WPE_EXPORT` | `WPE_API` (defined in `<wpe/WPEDefines.h>`) | Direct | Same role: visibility attribute. |
| Version macros (`WPE_MAJOR_VERSION`, etc.) | `<wpe/WPEVersion.h>` (generated; named the same) | Direct | |

---

## WPEBackend-fdo

Most of WPEBackend-fdo's public surface implemented the "exportable
view backend" pattern — where WPE WebKit produced rendered frames as
buffers and the embedder displayed them via callbacks. That direction
is inverted in WPEPlatform: the embedder (or built-in platform)
subclasses [class@View] and *receives* buffers via
[vfunc@View.render_buffer]. There is no longer a separate fdo-style
library between WebKit and the platform.

### `wpe/fdo.h`, `wpe/fdo-egl.h` (umbrella headers)

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `<wpe/fdo.h>` | `<wpe/wpe-platform.h>` | Reshape | |
| `<wpe/fdo-egl.h>` | `<wpe/wpe-platform.h>` (no EGL-specific umbrella; EGL display via [method@Display.get_egl_display]) | Reshape | |

### `wpe/initialize-egl.h`

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `wpe_fdo_initialize_for_egl_display(EGLDisplay)` | — | Removed | The display manages its own EGL initialisation. |

### `wpe/view-backend-exportable.h`, `wpe/view-backend-exportable-egl.h`

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_view_backend_exportable_fdo` | [class@View] subclass | Reshape | Subclass and override [vfunc@View.render_buffer] instead of registering an exportable backend. |
| `wpe_view_backend_exportable_fdo_create(client, data, w, h)` | Subclass [class@View], create with [ctor@View.new] | Reshape | |
| `wpe_view_backend_exportable_fdo_destroy()` | `g_object_unref()` | Direct | |
| `wpe_view_backend_exportable_fdo_get_view_backend()` | — | Direct | Same object — no wrapper. |
| `struct wpe_view_backend_exportable_fdo_client` (`export_buffer_resource`, `export_dmabuf_resource`, `export_shm_buffer`) | [vfunc@View.render_buffer] receiving [class@Buffer] subclasses | Reshape | Single virtual function receives whichever [class@Buffer] subclass the system uses. |
| `struct wpe_view_backend_exportable_fdo_dmabuf_resource` | [class@BufferDMABuf] | Reshape | First-class WPEPlatform type. |
| `struct wpe_view_backend_exportable_fdo_egl_client` (`export_egl_image`, `export_fdo_egl_image`, `export_shm_buffer`) | [vfunc@View.render_buffer] | Reshape | EGLImage-vs-DMA-BUF distinction is handled by the buffer subclass. |
| `wpe_view_backend_exportable_fdo_egl_create(...)` | Subclass + [ctor@View.new] | Reshape | |
| `wpe_view_backend_exportable_fdo_dispatch_frame_complete()` | [method@View.buffer_rendered] | Reshape | |
| `wpe_view_backend_exportable_fdo_dispatch_release_buffer()` | [method@View.buffer_released] | Reshape | |
| `wpe_view_backend_exportable_fdo_dispatch_release_shm_exported_buffer()` | [method@View.buffer_released] | Reshape | |
| `wpe_view_backend_exportable_fdo_egl_dispatch_release_image()` | [method@View.buffer_released] | Reshape | |
| `wpe_view_backend_exportable_fdo_egl_dispatch_release_exported_image()` | [method@View.buffer_released] | Reshape | |

### `wpe/exported-image-egl.h`

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_fdo_egl_exported_image` | [class@BufferDMABuf] | Reshape | The "image with width/height" abstraction is folded into the buffer class. |
| `wpe_fdo_egl_exported_image_get_width()` / `_get_height()` | [method@Buffer.get_width] / `get_height()` | Direct | |
| `wpe_fdo_egl_exported_image_get_egl_image()` | [method@Buffer.import_to_egl_image] | Reshape | |

### `wpe/exported-buffer-shm.h`

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| `struct wpe_fdo_shm_exported_buffer` | [class@BufferSHM] | Reshape | |
| `wpe_fdo_shm_exported_buffer_get_resource()` | — | Removed | Wayland `wl_resource` is no longer the contract; pixel data is via [method@BufferSHM.get_data]. |
| `wpe_fdo_shm_exported_buffer_get_shm_buffer()` | — | Removed | Same as above. |

### `wpe/unstable/*`

| WPEBackend-fdo symbol | WPEPlatform | Kind | Notes |
|---|---|---|---|
| DMA-BUF pool (`wpe_view_backend_dmabuf_pool_fdo`, `wpe_dmabuf_pool_entry`) | — | Removed | Not part of the WPEPlatform public surface. |
| EGLStream variant (`wpe_view_backend_exportable_fdo_eglstream*`, `wpe_fdo_initialize_eglstream()`) | — | Removed | EGLStream-specific path is gone. |
| Explicit SHM/DMA-BUF initialise calls (`wpe_fdo_initialize_dmabuf`, `_initialize_shm`) | — | Removed | The display picks its buffer types automatically. |

### `wpe/extensions/*`

These two extensions have no counterpart in the WPEPlatform public API:

- `wpe_audio_source` / `wpe_audio_receiver` (`<wpe/extensions/audio.h>`) — an external-audio *sink*: WebKit decodes the audio and streams raw PCM frames to the embedder over a shared file descriptor for it to render, instead of driving PulseAudio/ALSA. No WPEPlatform equivalent; audio uses WebKit's normal output path.
- `wpe_video_plane_display_dmabuf_source` / `_receiver` (`<wpe/extensions/video-plane-display-dmabuf.h>`) — zero-copy display of decoded video frames on a hardware overlay plane (Broadcom-class SoCs). A WPEPlatform equivalent is in progress — a Broadcom Nexus proof-of-concept has been prototyped — but the public API had not landed when this page was written.

---

## What's new in WPEPlatform with no libwpe / fdo predecessor

The following are first-class WPEPlatform APIs that did not exist in
the old stack at all:

- [class@Toplevel] (proper window abstraction; was implicit in the view backend).
- [class@Settings] (centralised platform/font/UX settings keyed by `WPE_SETTING_*` paths).
- [class@Screen] (proper screen/monitor object with sync observer for vblank).
- [class@ScreenSyncObserver] (vblank callbacks).
- [iface@GestureController] (interface for higher-level gesture recognition).
- [class@InputMethodContext] (IME with [enum@InputPurpose] / [flags@InputHints]).
- [iface@ViewAccessible] (accessibility binding interface).
- [class@BufferFormats] / [struct@BufferFormatsBuilder] (explicit format negotiation between WebKit and the platform).
- [struct@DRMDevice] (refcounted DRM device handle, used in format negotiation).
- [struct@Color], [struct@Rectangle] (small value types used across the API).
- Rendering-fence and release-fence FD plumbing on [class@Buffer] for explicit sync.
- DRM-specific `WPE_SETTING_DRM_SCALE` for tweaking DRM scanout scale.

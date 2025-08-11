#ifndef InspectorAPI_h
#define InspectorAPI_h

#include <JavaScriptCore/JSContextRef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback function type for receiving inspector messages.
 * @param message The JSON message from the inspector backend.
 */
typedef void (*InspectorMessageCallback)(const char* message);

/**
 * Enum representing the type of debugger pause-loop event.
 */
typedef enum {
    /** The debugger has entered paused state (breakpoint hit, debugger statement, etc.) */
    InspectorPauseEventPaused = 0,
    /** The debugger has exited paused state and resumed execution */
    InspectorPauseEventResumed = 1,
    /** Tick event during paused state nested run loop (for processing commands) */
    InspectorPauseEventTick = 2
} InspectorPauseEvent;

/**
 * Callback function type for debugger pause-loop events (paused/resumed/tick).
 *
 * @param ctx The JavaScript context (JSContextRef).
 * @param event The event type (Paused, Resumed, or Tick).
 */
typedef void (*InspectorPauseEventCallback)(JSContextRef ctx, InspectorPauseEvent event);

/**
 * Sets the callback to be called when the inspector sends messages.
 * This also connects the frontend channel to the inspector controller.
 * If a callback was previously set, the old frontend is disconnected first.
 * Passing NULL as the callback will disconnect the inspector.
 *
 * @param context The JavaScript context to configure.
 * @param callback The callback function to receive inspector messages, or NULL to disconnect.
 */
JS_EXPORT void JSInspectorSetCallback(JSGlobalContextRef context, InspectorMessageCallback callback);

/**
 * Sends a message to the inspector backend.
 * The message should be a valid JSON string following the WebKit Inspector Protocol.
 *
 * @param context The JavaScript context.
 * @param message The JSON message to send to the inspector.
 */
JS_EXPORT void JSInspectorSendMessage(JSGlobalContextRef context, const char* message);

/**
 * Disconnects the inspector frontend from the given context.
 * This should be called before releasing a context that has an active inspector connection.
 * After calling this function, no more inspector callbacks will be received for this context.
 *
 * @param context The JavaScript context to disconnect from the inspector.
 */
JS_EXPORT void JSInspectorDisconnect(JSGlobalContextRef context);

/**
 * Checks if the inspector is currently connected for the given context.
 *
 * @param context The JavaScript context to check.
 * @return true if an inspector frontend is connected, false otherwise.
 */
JS_EXPORT bool JSInspectorIsConnected(JSGlobalContextRef context);

/**
 * Sets the callback for debugger pause-loop events (paused/resumed/tick).
 *
 * This callback is invoked when:
 * - The debugger enters paused state (event = InspectorPauseEventPaused)
 * - The debugger exits paused state (event = InspectorPauseEventResumed)
 * - During the paused state nested run loop (event = InspectorPauseEventTick)
 *
 * The context (JSContextRef) is passed directly to the callback.
 *
 * @param context The JavaScript context to configure.
 * @param callback The pause-loop event callback, or NULL to disable.
 */
JS_EXPORT void JSInspectorSetPauseEventCallback(
    JSGlobalContextRef context,
    InspectorPauseEventCallback callback
);

#ifdef __cplusplus
}
#endif

#endif /* InspectorAPI_h */

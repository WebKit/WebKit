Native logs are often valuable in order to debug issues that can't be easily
reproduced. Following are instructions for gathering logs on various platforms.

To enable native logs for a native application, you can either:

  * Use a debug build of WebRTC (a build where `NDEBUG` is not defined),
    which will enable `INFO` logging by default.

  * Call `rtc::LogMessage::LogToDebug(rtc::LS_INFO)` within your application.
    Or use `LS_VERBOSE` to enable `VERBOSE` logging.

For the location of the log output on different platforms, see below.

#### Android

Logged to Android system log. Can be obtained using:

~~~~ bash
adb logcat -s "libjingle"
~~~~

To enable the logging in a non-debug build from Java code, use
`Logging.enableLogToDebugOutput(Logging.Severity.LS_INFO)`.

#### iOS

Only logged to `stderr` by default. To log to a file, use `RTCFileLogger`.

#### Mac

For debug builds of WebRTC (builds where `NDEBUG` is not defined), logs to
`stderr`. To do this for release builds as well, set a boolean preference named
'logToStderr' to `true` for your application. Or, use `RTCFileLogger` to log to
a file.

#### Windows

Logs to the debugger and `stderr`.

#### Linux/Other Platforms

Logs to `stderr`.

<!-- go/cmark -->

<!--* freshness: {owner: 'tommi' reviewed: '2026-04-15'} *-->

# WebRTC Logging Initialization

## Overview

This document explains how to initialize the WebRTC logging system, when it must
be done, and the consequences of late initialization.

## How to Initialize

To initialize logging with custom settings, call `webrtc::InitializeLogging` and
pass a `webrtc::LoggingConfig` object.

**Initialization can happen only once.**

Example:

```cpp
#include "rtc_base/logging.h"

int main() {
  webrtc::LoggingConfig config;
  config.set_min_severity(webrtc::LS_INFO);
  config.set_log_thread(true);
  config.set_log_timestamp(true);

  // Add custom sinks if needed
  // config.AddSink(std::make_unique<MyLogSink>());

  if (!webrtc::InitializeLogging(std::move(config))) {
    // Handle initialization failure (e.g., called too late)
  }

  // Proceed with WebRTC usage
}
```

## When Initialization Must Be Done

Initialization **must** be performed before any other WebRTC API calls and
before any logging occurs via `RTC_LOG` macros.

## Implicit Initialization

If not initialized explicitly, logging initialization will happen **implicitly**
upon the first logging call (e.g., `RTC_LOG`) or any function that accesses the
logging configuration. The defaults used for implicit initialization are defined
in the `LoggingConfig` struct.

## Behavior When It Is Too Late

If `InitializeLogging` is called after the logging system has already been
initialized (implicitly or explicitly):

1. The function will return `false`.
2. The provided `LoggingConfig` will be ignored.
3. The logging system will continue to operate with the configuration
   established by the first initialization.

To avoid this, ensure `InitializeLogging` is called as early as possible in the
application's startup sequence.

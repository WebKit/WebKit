#pragma once

#include <EGL/egl.h>
#include <stdint.h>

// Public functions are declared in trace_fixture.h.

// Private Functions

void SetupReplayContext8(void);
void ReplayFrame1(void);
void ReplayFrame2(void);
void ReplayFrame3(void);
void ReplayFrame4(void);
void ResetReplayContextShared(void);
void ResetReplayContext8(void);
void ReplayFrame5(void);
void SetupReplayContextShared(void);
void SetupReplayContextSharedInactive(void);
void InitReplay(void);

// Global variables


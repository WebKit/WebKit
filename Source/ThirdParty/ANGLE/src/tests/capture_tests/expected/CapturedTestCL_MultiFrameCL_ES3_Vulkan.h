#pragma once

#define CL_NO_EXTENSION_PROTOTYPES
#include <angle_cl.h>
#include <stdint.h>
#include "trace_fixture_cl.h"

// Public functions are declared in trace_fixture_cl.h.

// Private Functions

void ReplayFrame2(void);
void ReplayFrame3(void);
void ReplayFrame4(void);
void ReplayFrame5(void);
void ReplayFrame6(void);
void InitReplay(void);

// Global variables

extern const char * clCreateProgramWithSource_strings_0[];
static clIcdGetPlatformIDsKHR_fn clIcdGetPlatformIDsKHR;
static clEnqueueAcquireExternalMemObjectsKHR_fn clEnqueueAcquireExternalMemObjectsKHR;
static clEnqueueReleaseExternalMemObjectsKHR_fn clEnqueueReleaseExternalMemObjectsKHR;
static clImportMemoryARM_fn clImportMemoryARM;

#ifndef _OpenGLESAvailability_H
#define _OpenGLESAvailability_H

#include <os/availability.h>


#ifdef GLES_SILENCE_DEPRECATION
  #define OPENGLES_DEPRECATED(...)
#else
  #define OPENGLES_DEPRECATED(...) API_DEPRECATED("OpenGLES API deprecated. (Define GLES_SILENCE_DEPRECATION to silence these warnings)", __VA_ARGS__)
#endif

#endif

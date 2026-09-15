/*
	Copyright:	(c) 2010-2012 Apple Inc. All rights reserved.
*/
#ifndef __gltypes_h_
#define __gltypes_h_

#include <stdint.h>

typedef uint32_t GLbitfield;
typedef uint8_t  GLboolean;
typedef int8_t   GLbyte;
typedef float    GLclampf;
typedef uint32_t GLenum;
typedef float    GLfloat;
typedef int32_t  GLint;
typedef int16_t  GLshort;
typedef int32_t  GLsizei;
typedef uint8_t  GLubyte;
typedef uint32_t GLuint;
typedef uint16_t GLushort;
typedef void     GLvoid;

#if !defined(GL_ES_VERSION_2_0)
typedef char     GLchar;
#endif
typedef int32_t  GLclampx;
typedef int32_t  GLfixed;
#if !defined(GL_ES_VERSION_3_0)
typedef uint16_t GLhalf;
#endif
#if !defined(GL_APPLE_sync) && !defined(GL_ES_VERSION_3_0)
typedef int64_t  GLint64;
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
#endif
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;

#endif

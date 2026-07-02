/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief OpenGL wrapper base types.
 *
 * \note Unlike most .inl files this one is NOT auto-generated. This is .inl
 *		 because it is included from both glwDefs.hpp (inside glw namespace)
 *		 and glw.h (in root namespace, as C source).
 *//*--------------------------------------------------------------------*/

/* Calling convention. */
#if (DE_OS == DE_OS_ANDROID)
#	include <sys/cdefs.h>
#	if !defined(__NDK_FPABI__)
#		define __NDK_FPABI__
#	endif
#	define GLW_APICALL __NDK_FPABI__
#else
#	define GLW_APICALL
#endif

#if (DE_OS == DE_OS_WIN32)
#	define GLW_APIENTRY __stdcall
#else
#	define GLW_APIENTRY
#endif

/* Signed basic types. */
typedef int8_t				GLbyte;
typedef int16_t				GLshort;
typedef int32_t				GLint;
typedef int64_t				GLint64;

/* Unsigned basic types. */
typedef uint8_t				GLubyte;
typedef uint16_t			GLushort;
typedef uint32_t			GLuint;
typedef uint64_t			GLuint64;

/* Floating-point types. */
typedef uint16_t			GLhalf;
typedef float				GLfloat;
typedef float				GLclampf;
typedef double				GLdouble;
typedef double				GLclampd;

/* Special types. */
typedef char				GLchar;
typedef uint8_t				GLboolean;
typedef uint32_t			GLenum;
typedef uint32_t			GLbitfield;
typedef int32_t				GLsizei;
typedef int32_t				GLfixed;
typedef void				GLvoid;

#if (DE_OS == DE_OS_WIN32 && DE_CPU == DE_CPU_X86_64)
	typedef signed long long int	GLintptr;
	typedef signed long long int	GLsizeiptr;
#else
	typedef signed long int			GLintptr;
	typedef signed long int			GLsizeiptr;
#endif

/* Opaque handles. */
typedef struct __GLsync*	GLsync;
typedef void*				GLeglImageOES;

/* Callback for GL_ARB_debug_output. */
typedef void (GLW_APIENTRY* GLDEBUGPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const GLvoid *userParam);

/* OES_EGL_image */
typedef void*				GLeglImageOES;

/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Utilities
 * ------------------------------------------
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
 * \brief EGL wrapper base types.
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
#	define EGLW_APICALL __NDK_FPABI__
#else
#	define EGLW_APICALL
#endif

#if (DE_OS == DE_OS_WIN32)
#	define EGLW_APIENTRY __stdcall
#else
#	define EGLW_APIENTRY
#endif

typedef int32_t		EGLint;
typedef uint32_t	EGLenum;
typedef uint32_t	EGLBoolean;
typedef intptr_t	EGLAttrib;
typedef uint64_t	EGLTime;

typedef void*		EGLDisplay;
typedef void*		EGLConfig;
typedef void*		EGLSurface;
typedef void*		EGLContext;
typedef void*		EGLImage;
typedef void*		EGLClientBuffer;
typedef void*		EGLSync;

typedef void*		EGLNativeDisplayType;
typedef void*		EGLNativeWindowType;
typedef void*		EGLNativePixmapType;

typedef void (*__eglMustCastToProperFunctionPointerType) (void);

typedef EGLImage	EGLImageKHR;
typedef EGLSync		EGLSyncKHR;
typedef EGLTime		EGLTimeKHR;

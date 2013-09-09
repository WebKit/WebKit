/*
*  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef WTF_PlatformNix_h
#define WTF_PlatformNix_h

#define WTF_PLATFORM_NIX 1
#define ENABLE_GLOBAL_FASTMALLOC_NEW 0

#define WTF_USE_3D_GRAPHICS 1
#define WTF_USE_ACCELERATED_COMPOSITING 1
#define WTF_USE_CAIRO 1
#define WTF_USE_COORDINATED_GRAPHICS 1
#define WTF_USE_CROSS_PLATFORM_CONTEXT_MENUS 1
#define WTF_USE_FREETYPE 1
#define WTF_USE_GLIB 1
#define WTF_USE_HARFBUZZ 1
#define WTF_USE_HARFBUZZ_NG 1
#define WTF_USE_ICU_UNICODE 1
#define WTF_USE_IOSURFACE_CANVAS_BACKING_STORE 1
#define WTF_USE_LEVELDB 1
#define WTF_USE_PTHREADS 1
#define WTF_USE_TEXTURE_MAPPER 1
#define WTF_USE_TEXTURE_MAPPER_GL 1
#define WTF_USE_TILED_BACKING_STORE 1
#define WTF_USE_UNIX_DOMAIN_SOCKETS 1

#if !defined(WTF_USE_OPENGL_ES_2) || !WTF_USE_OPENGL_ES_2
#define WTF_USE_OPENGL 1
#define WTF_USE_GLX 1
#define WTF_PLATFORM_X11 1
#define WTF_USE_GRAPHICS_SURFACE 1
#else
#define WTF_USE_EGL 1
#endif

#if !defined(WTF_USE_CURL) || !WTF_USE_CURL
#define WTF_USE_SOUP 1
#endif

#endif

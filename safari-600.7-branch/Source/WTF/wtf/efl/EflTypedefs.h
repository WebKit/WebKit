/*
 * Copyright (C) 2014 SAMSUNG ELECTRONICS All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef WTF_EflTypedefs_h
#define WTF_EflTypedefs_h

#ifdef __cplusplus

#if PLATFORM(EFL)
typedef unsigned Ecore_X_ID;
typedef Ecore_X_ID Ecore_X_Window;

typedef unsigned char Eina_Bool;
typedef struct _Evas_Point Evas_Point;
typedef struct _Evas_GL Evas_GL;
typedef struct _Evas_GL_Context Evas_GL_Context;
typedef struct _Evas_GL_Surface Evas_GL_Surface;
typedef struct _Ecore_Evas Ecore_Evas;
typedef struct _Ecore_Fd_Handler Ecore_Fd_Handler;
typedef struct _Eina_List Eina_List;
typedef struct _Eina_Module Eina_Module;
typedef struct _Eina_Rectangle Eina_Rectangle;
#if USE(EO)
typedef struct _Eo_Opaque Evas;
typedef struct _Eo_Opaque Evas_Object;
typedef struct _Eo_Opaque Ecore_Timer;
#else
typedef struct _Evas Evas;
typedef struct _Evas_Object Evas_Object;
typedef struct _Ecore_Timer Ecore_Timer;
#endif
#endif

#endif

#endif // WTF_EflTypedefs_h

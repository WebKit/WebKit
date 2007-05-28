/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 

#ifndef NotImplemented_h
#define NotImplemented_h
 
#ifdef NDEBUG
 
#define notImplemented() ((void)0)
 
#else
 
#if PLATFORM(GDK)
     
    #define notImplemented() do { \
       static bool printed = false; \
       if (!printed && !getenv("DISABLE_NI_WARNING")) { \
           fprintf(stderr, "FIXME: UNIMPLEMENTED %s %s:%d\n", WTF_PRETTY_FUNCTION, __FILE__, __LINE__); \
           printed = true; \
       } \
    } while (0)
     
#elif PLATFORM(QT)

    #include <QApplication>
    #define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, WTF_PRETTY_FUNCTION)

#else
    
    #if COMPILER(GCC)
    #define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED %s %s:%d\n", WTF_PRETTY_FUNCTION, __FILE__, __LINE__); } while(0)
    #endif

    #if COMPILER(MSVC)
    #define notImplemented() do { \
        char buf[256] = {0}; \
        _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
        OutputDebugStringA(buf); \
    } while (0)
    #endif

#endif // PLATFORM(GDK)

#endif // NDEBUG

#endif // NotImplemented_h

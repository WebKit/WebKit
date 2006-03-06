// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef KXMLCORE_PLATFORM_H
#define KXMLCORE_PLATFORM_H

// PLATFORM handles OS, operating environment, graphics API, and CPU
#define PLATFORM(KX_FEATURE) (defined( KXMLCORE_PLATFORM_##KX_FEATURE ) && KXMLCORE_PLATFORM_##KX_FEATURE)
#define COMPILER(KX_FEATURE) (defined( KXMLCORE_COMPILER_##KX_FEATURE ) && KXMLCORE_COMPILER_##KX_FEATURE)
#define HAVE(KX_FEATURE) (defined( HAVE_##KX_FEATURE ) && HAVE_##KX_FEATURE)
#define USE(KX_FEATURE) (defined( KXMLCORE_USE_##KX_FEATURE ) && KXMLCORE_USE_##KX_FEATURE)

// Operating systems - low-level dependencies

// PLATFORM(DARWIN)
// Operating system level dependencies for Mac OS X / Darwin that should
// be used regardless of operating environment
#ifdef __APPLE__
#define KXMLCORE_PLATFORM_DARWIN 1
#endif

// PLATFORM(WIN_OS)
// Operating system level dependencies for Windows that should be used
// regardless of operating environment
#if defined(WIN32) || defined(_WIN32)
#define KXMLCORE_PLATFORM_WIN_OS 1
#endif

// PLATFORM(UNIX)
// Operating system level dependencies for Unix-like systems that
// should be used regardless of operating environment
// (includes PLATFORM(DARWIN))
#if   defined(__APPLE__)   \
   || defined(unix)        \
   || defined(__unix)      \
   || defined(__unix__)    \
   || defined (__NetBSD__) \
   || defined(_AIX)
#define KXMLCORE_PLATFORM_UNIX 1
#endif

// Operating environments

#define KXMLCORE_HAVE_OPERATING_ENVIRONMENT 0

// PLATFORM(KDE)
// Operating environment dependencies for KDE
// I made this macro up for the KDE build system to define
#if defined(BUILDING_KDE__)
#define KXMLCORE_PLATFORM_KDE 1
#undef KXMLCORE_HAVE_OPERATING_ENVIRONMENT
#define KXMLCORE_HAVE_OPERATING_ENVIRONMENT 1
#endif

#if !KXMLCORE_HAVE_OPERATING_ENVIRONMENT && PLATFORM(DARWIN)
#define KXMLCORE_PLATFORM_MAC 1
#undef KXMLCORE_HAVE_OPERATING_ENVIRONMENT
#define KXMLCORE_HAVE_OPERATING_ENVIRONMENT 1
#endif

#if !KXMLCORE_HAVE_OPERATING_ENVIRONMENT && PLATFORM(WIN_OS)
#define KXMLCORE_PLATFORM_WIN 1
#undef KXMLCORE_HAVE_OPERATING_ENVIRONMENT
#define KXMLCORE_HAVE_OPERATING_ENVIRONMENT 1
#endif

// CPU

// PLATFORM(PPC)
#if   defined(__ppc__)     \
   || defined(__PPC__)     \
   || defined(__powerpc__) \
   || defined(__powerpc)   \
   || defined(__POWERPC__) \
   || defined(_M_PPC)      \
   || defined(__PPC)
#define KXMLCORE_PLATFORM_PPC 1
#define KXMLCORE_PLATFORM_BIG_ENDIAN 1
#endif

// PLATFORM(PPC64)
#if   defined(__ppc64__) \
   || defined(__PPC64__)
#define KXMLCORE_PLATFORM_PPC64 1
#define KXMLCORE_PLATFORM_BIG_ENDIAN 1
#endif

#if defined(arm)
#define KXMLCORE_PLATFORM_ARM 1
#define KXMLCORE_PLATFORM_MIDDLE_ENDIAN 1
#endif

// PLATFORM(X86)
#if   defined(__i386__) \
   || defined(i386)     \
   || defined(_M_IX86)  \
   || defined(_X86_)    \
   || defined(__THW_INTEL)
#define KXMLCORE_PLATFORM_X86 1
#endif

// Compiler

// COMPILER(MSVC)
#if defined(_MSC_VER)
#define KXMLCORE_COMPILER_MSVC 1
#endif

// COMPILER(GCC)
#if defined(__GNUC__)
#define KXMLCORE_COMPILER_GCC 1
#endif

// COMPILER(BORLAND)
// not really fully supported - is this relevant any more?
#if defined(__BORLANDC__)
#define KXMLCORE_COMPILER_BORLAND 1
#endif

// COMPILER(CYGWIN)
// not really fully supported - is this relevant any more?
#if defined(__CYGWIN__)
#define KXMLCORE_COMPILER_CYGWIN 1
#endif

// multiple threads only supported on OS X WebKit for now
#if PLATFORM(MAC)
#define KXMLCORE_USE_MULTIPLE_THREADS 1
#endif

#endif // KXMLCORE_PLATFORM_H

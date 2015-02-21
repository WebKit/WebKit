/*
 * Copyright (C) 2007, 2009-2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SoftLinking_h
#define SoftLinking_h

#include <windows.h>
#include <wtf/Assertions.h>

#pragma mark - Soft-link helper macros

#define SOFT_LINK_LIBRARY_HELPER(lib, suffix) \
    static HMODULE lib##Library() \
    { \
        static HMODULE library = LoadLibraryW(L###lib suffix); \
        return library; \
    }

#define SOFT_LINK_GETPROCADDRESS GetProcAddress
#define SOFT_LINK_LIBRARY(lib) SOFT_LINK_LIBRARY_HELPER(lib, L".dll")
#define SOFT_LINK_DEBUG_LIBRARY(lib) SOFT_LINK_LIBRARY_HELPER(lib, L"_debug.dll")

#pragma mark - Soft-link macros for use within a single source file

#define SOFT_LINK(library, functionName, resultType, callingConvention, parameterDeclarations, parameterNames) \
    static resultType callingConvention init##functionName parameterDeclarations; \
    static resultType (callingConvention*softLink##functionName) parameterDeclarations = init##functionName; \
    \
    static resultType callingConvention init##functionName parameterDeclarations \
    { \
        softLink##functionName = reinterpret_cast<resultType (callingConvention*) parameterDeclarations>(SOFT_LINK_GETPROCADDRESS(library##Library(), #functionName)); \
        ASSERT(softLink##functionName); \
        return softLink##functionName parameterNames; \
    } \
    \
    inline resultType functionName parameterDeclarations \
    { \
        return softLink##functionName parameterNames; \
    }

#define SOFT_LINK_OPTIONAL(library, functionName, resultType, callingConvention, parameterDeclarations) \
    typedef resultType (callingConvention *functionName##PtrType) parameterDeclarations; \
    static functionName##PtrType functionName##Ptr() \
    { \
        static functionName##PtrType ptr; \
        static bool initialized; \
        \
        if (initialized) \
            return ptr; \
        initialized = true; \
        \
        ptr = reinterpret_cast<functionName##PtrType>(SOFT_LINK_GETPROCADDRESS(library##Library(), #functionName)); \
        return ptr; \
    } \

#define SOFT_LINK_LOADED_LIBRARY(library, functionName, resultType, callingConvention, parameterDeclarations) \
    typedef resultType (callingConvention *functionName##PtrType) parameterDeclarations; \
    static functionName##PtrType functionName##Ptr() \
    { \
        static functionName##PtrType ptr; \
        static bool initialized; \
        \
        if (initialized) \
            return ptr; \
        initialized = true; \
        \
        static HINSTANCE libraryInstance = ::GetModuleHandle(L#library); \
        \
        ptr = reinterpret_cast<functionName##PtrType>(SOFT_LINK_GETPROCADDRESS(libraryInstance, #functionName)); \
        return ptr; \
    } \

/*
    In order to soft link against functions decorated with __declspec(dllimport), we prepend "softLink_" to the function names.
    If you use SOFT_LINK_DLL_IMPORT(), you will also need to #define the function name to account for this, e.g.:

    SOFT_LINK_DLL_IMPORT(myLibrary, myFunction, ...)
    #define myFunction softLink_myFunction
*/
#define SOFT_LINK_DLL_IMPORT(library, functionName, resultType, callingConvention, parameterDeclarations, parameterNames) \
    static resultType callingConvention init##functionName parameterDeclarations; \
    static resultType(callingConvention*softLink##functionName) parameterDeclarations = init##functionName; \
    \
    static resultType callingConvention init##functionName parameterDeclarations \
    { \
        softLink##functionName = reinterpret_cast<resultType (callingConvention*)parameterDeclarations>(SOFT_LINK_GETPROCADDRESS(library##Library(), #functionName)); \
        ASSERT(softLink##functionName); \
        return softLink##functionName parameterNames; \
    } \
    \
    inline resultType softLink_##functionName parameterDeclarations \
    { \
        return softLink##functionName parameterNames; \
    }

#define SOFT_LINK_DLL_IMPORT_OPTIONAL(library, functionName, resultType, callingConvention, parameterDeclarations) \
    typedef resultType (callingConvention *functionName##PtrType) parameterDeclarations; \
    static functionName##PtrType functionName##Ptr() \
    { \
        static functionName##PtrType ptr; \
        static bool initialized; \
        \
        if (initialized) \
            return ptr; \
        initialized = true; \
        \
        ptr = reinterpret_cast<resultType(callingConvention*)parameterDeclarations>(SOFT_LINK_GETPROCADDRESS(library##Library(), #functionName)); \
        return ptr; \
    } \

#define SOFT_LINK_DLL_IMPORT_OPTIONAL(library, functionName, resultType, callingConvention, parameterDeclarations) \
    typedef resultType (callingConvention *functionName##PtrType) parameterDeclarations; \
    static functionName##PtrType functionName##Ptr() \
    { \
        static functionName##PtrType ptr; \
        static bool initialized; \
        \
        if (initialized) \
            return ptr; \
        initialized = true; \
        \
        ptr = reinterpret_cast<resultType(callingConvention*)parameterDeclarations>(SOFT_LINK_GETPROCADDRESS(library##Library(), #functionName)); \
        return ptr; \
    } \

/*
    Variables exported by a DLL need to be accessed through a function.
    If you use SOFT_LINK_VARIABLE_DLL_IMPORT(), you will also need to #define the variable name to account for this, e.g.:

    SOFT_LINK_VARIABLE_DLL_IMPORT(myLibrary, myVar, int)
    #define myVar get_myVar()
*/
#define SOFT_LINK_VARIABLE_DLL_IMPORT(library, variableName, variableType) \
    static variableType get##variableName() \
    { \
        static variableType* ptr = reinterpret_cast<variableType*>(SOFT_LINK_GETPROCADDRESS(library##Library(), #variableName)); \
        ASSERT(ptr); \
        return *ptr; \
    } \

/*
    Note that this will only work for variable types for which a return value of 0 can signal an error.
 */
#define SOFT_LINK_VARIABLE_DLL_IMPORT_OPTIONAL(library, variableName, variableType) \
    static variableType get##variableName() \
    { \
        static variableType* ptr = reinterpret_cast<variableType*>(SOFT_LINK_GETPROCADDRESS(library##Library(), #variableName)); \
        if (!ptr) \
            return 0; \
        return *ptr; \
    } \

#pragma mark - Soft-link macros for sharing across multiple source files

// See Source/WebCore/platform/cf/CoreMediaSoftLink.{cpp,h} for an example implementation.

#ifdef DEBUG_ALL
#define SOFT_LINK_FRAMEWORK_SOURCE(functionNamespace, framework) \
    namespace functionNamespace { \
    SOFT_LINK_DEBUG_LIBRARY(framework) \
    }
#else
#define SOFT_LINK_FRAMEWORK_SOURCE(functionNamespace, framework) \
    namespace functionNamespace { \
    SOFT_LINK_LIBRARY(framework) \
    }
#endif

#define SOFT_LINK_FUNCTION_HEADER(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    namespace functionNamespace { \
    extern resultType(__cdecl*softLink##framework##functionName) parameterDeclarations; \
    inline resultType softLink_##framework##_##functionName parameterDeclarations \
    { \
        return softLink##framework##functionName parameterNames; \
    } \
    }

#define SOFT_LINK_FUNCTION_SOURCE(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    namespace functionNamespace { \
    static resultType __cdecl init##framework##functionName parameterDeclarations; \
    resultType(__cdecl*softLink##framework##functionName) parameterDeclarations = init##framework##functionName; \
    static resultType __cdecl init##framework##functionName parameterDeclarations \
    { \
        softLink##framework##functionName = reinterpret_cast<resultType (__cdecl*)parameterDeclarations>(SOFT_LINK_GETPROCADDRESS(framework##Library(), #functionName)); \
        ASSERT(softLink##framework##functionName); \
        return softLink##framework##functionName parameterNames; \
    } \
    }

#endif // SoftLinking_h

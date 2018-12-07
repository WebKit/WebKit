/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/Assertions.h>
#import <dlfcn.h>
#import <objc/runtime.h>

#pragma mark - Soft-link macros for use within a single source file

#define SOFT_LINK_LIBRARY(lib) \
    static void* lib##Library() \
    { \
        static void* dylib = ^{ \
            void *result = dlopen("/usr/lib/" #lib ".dylib", RTLD_NOW); \
            RELEASE_ASSERT_WITH_MESSAGE(result, "%s", dlerror()); \
            return result; \
        }(); \
        return dylib; \
    }

#define SOFT_LINK_FRAMEWORK(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = ^{ \
            void* result = dlopen("/System/Library/Frameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            RELEASE_ASSERT_WITH_MESSAGE(result, "%s", dlerror()); \
            return result; \
        }(); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_PRIVATE_FRAMEWORK(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = ^{ \
            void* result = dlopen("/System/Library/PrivateFrameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            RELEASE_ASSERT_WITH_MESSAGE(result, "%s", dlerror()); \
            return result; \
        }(); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_FRAMEWORK_OPTIONAL_PREFLIGHT(framework) \
    static bool framework##LibraryIsAvailable() \
    { \
        static bool frameworkLibraryIsAvailable = dlopen_preflight("/System/Library/Frameworks/" #framework ".framework/" #framework); \
        return frameworkLibraryIsAvailable; \
    }

#define SOFT_LINK_FRAMEWORK_OPTIONAL(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = dlopen("/System/Library/Frameworks/" #framework ".framework/" #framework, RTLD_NOW); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = dlopen("/System/Library/PrivateFrameworks/" #framework ".framework/" #framework, RTLD_NOW); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_STAGED_FRAMEWORK(framework, unstagedLocation, version) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = ^{ \
            void* result = dlopen("/System/Library/" #unstagedLocation "/" #framework ".framework/Versions/" #version "/" #framework, RTLD_LAZY); \
            if (!result) \
                result = dlopen("/System/Library/StagedFrameworks/Safari/" #framework ".framework/Versions/" #version "/" #framework, RTLD_LAZY); \
            RELEASE_ASSERT_WITH_MESSAGE(result, "%s", dlerror()); \
            return result; \
        }(); \
        return frameworkLibrary; \
    }

#define SOFT_LINK_FRAMEWORK_IN_UMBRELLA(umbrella, framework) \
    static void* framework##Library() \
    { \
        static void* frameworkLibrary = ^{ \
            void* result = dlopen("/System/Library/Frameworks/" #umbrella ".framework/Frameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            RELEASE_ASSERT_WITH_MESSAGE(result, "%s", dlerror()); \
            return result; \
        }(); \
        return frameworkLibrary; \
    }

#define SOFT_LINK(framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    static resultType init##functionName parameterDeclarations; \
    static resultType (*softLink##functionName) parameterDeclarations = init##functionName; \
    \
    static resultType init##functionName parameterDeclarations \
    { \
        softLink##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
        RELEASE_ASSERT_WITH_MESSAGE(softLink##functionName, "%s", dlerror()); \
        return softLink##functionName parameterNames; \
    } \
    \
    inline __attribute__((__always_inline__)) resultType functionName parameterDeclarations \
    { \
        return softLink##functionName parameterNames; \
    }

#define SOFT_LINK_MAY_FAIL(framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    static resultType (*softLink##functionName) parameterDeclarations = 0; \
    \
    static bool init##functionName() \
    { \
        ASSERT(!softLink##functionName); \
        softLink##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
        return !!softLink##functionName; \
    } \
    \
    static bool canLoad##functionName() \
    { \
        static bool loaded = init##functionName(); \
        return loaded; \
    } \
    \
    inline __attribute__((__always_inline__)) __attribute__((visibility("hidden"))) resultType functionName parameterDeclarations \
    { \
        ASSERT(softLink##functionName); \
        return softLink##functionName parameterNames; \
    }

/* callingConvention is unused on Mac but is here to keep the macro prototype the same between Mac and Windows. */
#define SOFT_LINK_OPTIONAL(framework, functionName, resultType, callingConvention, parameterDeclarations) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    typedef resultType (*functionName##PtrType) parameterDeclarations; \
    \
    static functionName##PtrType functionName##Ptr() \
    { \
        static functionName##PtrType ptr = reinterpret_cast<functionName##PtrType>(dlsym(framework##Library(), #functionName)); \
        return ptr; \
    }

#define SOFT_LINK_CLASS(framework, className) \
    @class className; \
    static Class init##className(); \
    static Class (*get##className##Class)() = init##className; \
    static Class class##className; \
    \
    static Class className##Function() \
    { \
        return class##className; \
    } \
    \
    static Class init##className() \
    { \
        framework##Library(); \
        class##className = objc_getClass(#className); \
        RELEASE_ASSERT(class##className); \
        get##className##Class = className##Function; \
        return class##className; \
    } \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wunused-function\"") \
    static className *alloc##className##Instance() \
    { \
        return [get##className##Class() alloc]; \
    } \
    _Pragma("clang diagnostic pop")

#define SOFT_LINK_CLASS_OPTIONAL(framework, className) \
    @class className; \
    static Class init##className(); \
    static Class (*get##className##Class)() = init##className; \
    static Class class##className; \
    \
    static Class className##Function() \
    { \
        return class##className; \
    } \
    \
    static Class init##className() \
    { \
        framework##Library(); \
        class##className = objc_getClass(#className); \
        get##className##Class = className##Function; \
        return class##className; \
    } \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wunused-function\"") \
    static className *alloc##className##Instance() \
    { \
        return [get##className##Class() alloc]; \
    } \
    _Pragma("clang diagnostic pop")

#define SOFT_LINK_POINTER(framework, name, type) \
    static type init##name(); \
    static type (*get##name)() = init##name; \
    static type pointer##name; \
    \
    static type name##Function() \
    { \
        return pointer##name; \
    } \
    \
    static type init##name() \
    { \
        void** pointer = static_cast<void**>(dlsym(framework##Library(), #name)); \
        RELEASE_ASSERT_WITH_MESSAGE(pointer, "%s", dlerror()); \
        pointer##name = static_cast<type>(*pointer); \
        get##name = name##Function; \
        return pointer##name; \
    }

#define SOFT_LINK_POINTER_OPTIONAL(framework, name, type) \
    static type init##name(); \
    static type (*get##name)() = init##name; \
    static type pointer##name; \
    \
    static type name##Function() \
    { \
        return pointer##name; \
    } \
    \
    static type init##name() \
    { \
        void** pointer = static_cast<void**>(dlsym(framework##Library(), #name)); \
        if (pointer) \
            pointer##name = static_cast<type>(*pointer); \
        get##name = name##Function; \
        return pointer##name; \
    }

#define SOFT_LINK_CONSTANT(framework, name, type) \
    static type init##name(); \
    static type (*get##name)() = init##name; \
    static type constant##name; \
    \
    static type name##Function() \
    { \
        return constant##name; \
    } \
    \
    static type init##name() \
    { \
        void* constant = dlsym(framework##Library(), #name); \
        RELEASE_ASSERT_WITH_MESSAGE(constant, "%s", dlerror()); \
        constant##name = *static_cast<type const *>(constant); \
        get##name = name##Function; \
        return constant##name; \
    }

#define SOFT_LINK_CONSTANT_MAY_FAIL(framework, name, type) \
    static bool init##name(); \
    static type (*get##name)() = 0; \
    static type constant##name; \
    \
    static type name##Function() \
    { \
        return constant##name; \
    } \
    \
    static bool canLoad##name() \
    { \
        static bool loaded = init##name(); \
        return loaded; \
    } \
    \
    static bool init##name() \
    { \
        ASSERT(!get##name); \
        void* constant = dlsym(framework##Library(), #name); \
        if (!constant) \
            return false; \
        constant##name = *static_cast<type const *>(constant); \
        get##name = name##Function; \
        return true; \
    }

#pragma mark - Soft-link macros for sharing across multiple source files

// See Source/WebCore/platform/cf/CoreMediaSoftLink.{cpp,h} for an example implementation.


#define SOFT_LINK_LIBRARY_FOR_HEADER(functionNamespace, lib) \
    namespace functionNamespace { \
    extern void* lib##Library(bool isOptional = false); \
    inline bool is##lib##LibaryAvailable() { \
        return lib##Library(true) != nullptr; \
    } \
    }

#define SOFT_LINK_LIBRARY_FOR_SOURCE(functionNamespace, lib) \
    namespace functionNamespace { \
    void* lib##Library(bool isOptional); \
    void* lib##Library(bool isOptional) \
    { \
        static void* library; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            library = dlopen("/usr/lib/" #lib ".dylib", RTLD_NOW); \
            if (!isOptional) \
                RELEASE_ASSERT_WITH_MESSAGE(library, "%s", dlerror()); \
        }); \
        return library; \
    } \
    }

#define SOFT_LINK_FRAMEWORK_FOR_HEADER(functionNamespace, framework) \
    namespace functionNamespace { \
    extern void* framework##Library(bool isOptional = false); \
    bool is##framework##FrameworkAvailable(); \
    inline bool is##framework##FrameworkAvailable() { \
        return framework##Library(true) != nullptr; \
    } \
    }

#define SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, export) \
    namespace functionNamespace { \
    export void* framework##Library(bool isOptional = false); \
    void* framework##Library(bool isOptional) \
    { \
        static void* frameworkLibrary; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            frameworkLibrary = dlopen("/System/Library/Frameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            if (!isOptional) \
                RELEASE_ASSERT_WITH_MESSAGE(frameworkLibrary, "%s", dlerror()); \
        }); \
        return frameworkLibrary; \
    } \
    }

#define SOFT_LINK_FRAMEWORK_FOR_SOURCE(functionNamespace, framework) \
    SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, )

#define SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, export) \
    namespace functionNamespace { \
    export void* framework##Library(bool isOptional = false); \
    void* framework##Library(bool isOptional) \
    { \
        static void* frameworkLibrary; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            frameworkLibrary = dlopen("/System/Library/PrivateFrameworks/" #framework ".framework/" #framework, RTLD_NOW); \
            if (!isOptional) \
                RELEASE_ASSERT_WITH_MESSAGE(frameworkLibrary, "%s", dlerror()); \
        }); \
        return frameworkLibrary; \
    } \
    }

#define SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(functionNamespace, framework) \
    SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, )

#define SOFT_LINK_CLASS_FOR_HEADER(functionNamespace, className) \
    @class className; \
    namespace functionNamespace { \
    extern Class (*get##className##Class)(); \
    className *alloc##className##Instance(); \
    inline className *alloc##className##Instance() \
    { \
        return [get##className##Class() alloc]; \
    } \
    }

#define SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_ASSERTION(functionNamespace, framework, className, export, assertion) \
    @class className; \
    namespace functionNamespace { \
    static Class init##className(); \
    export Class (*get##className##Class)() = init##className; \
    static Class class##className; \
    \
    static Class className##Function() \
    { \
        return class##className; \
    } \
    \
    static Class init##className() \
    { \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            framework##Library(); \
            class##className = objc_getClass(#className); \
            assertion(class##className); \
            get##className##Class = className##Function; \
        }); \
        return class##className; \
    } \
    }

#define NO_ASSERT(assertion) (void(0))

#define SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, className, export) \
    SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_ASSERTION(functionNamespace, framework, className, export, RELEASE_ASSERT)

#define SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL_WITH_EXPORT(functionNamespace, framework, className, export) \
    SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_ASSERTION(functionNamespace, framework, className, export, NO_ASSERT)

#define SOFT_LINK_CLASS_FOR_SOURCE(functionNamespace, framework, className) \
    SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_ASSERTION(functionNamespace, framework, className, , RELEASE_ASSERT)

#define SOFT_LINK_CLASS_FOR_SOURCE_OPTIONAL(functionNamespace, framework, className) \
    SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_ASSERTION(functionNamespace, framework, className, , NO_ASSERT)

#define SOFT_LINK_CONSTANT_FOR_HEADER(functionNamespace, framework, variableName, variableType) \
    WTF_EXTERN_C_BEGIN \
    extern const variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    variableType get_##framework##_##variableName(); \
    }

#define SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, variableName, variableType, export) \
    WTF_EXTERN_C_BEGIN \
    extern const variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    export variableType get_##framework##_##variableName(); \
    variableType get_##framework##_##variableName() \
    { \
        static variableType constant##framework##variableName; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            void* constant = dlsym(framework##Library(), #variableName); \
            RELEASE_ASSERT_WITH_MESSAGE(constant, "%s", dlerror()); \
            constant##framework##variableName = *static_cast<variableType const *>(constant); \
        }); \
        return constant##framework##variableName; \
    } \
    }

#define SOFT_LINK_CONSTANT_FOR_SOURCE(functionNamespace, framework, variableName, variableType) \
    SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, variableName, variableType, )

#define SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(functionNamespace, framework, variableName, variableType) \
    WTF_EXTERN_C_BEGIN \
    extern const variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    bool canLoad_##framework##_##variableName(); \
    bool init_##framework##_##variableName(); \
    variableType get_##framework##_##variableName(); \
    }

#define SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(functionNamespace, framework, variableName, variableType) \
    WTF_EXTERN_C_BEGIN \
    extern const variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    static variableType constant##framework##variableName; \
    bool init_##framework##_##variableName(); \
    bool init_##framework##_##variableName() \
    { \
        void* constant = dlsym(framework##Library(), #variableName); \
        if (!constant) \
            return false; \
        constant##framework##variableName = *static_cast<variableType const *>(constant); \
        return true; \
    } \
    bool canLoad_##framework##_##variableName(); \
    bool canLoad_##framework##_##variableName() \
    { \
        static bool loaded = init_##framework##_##variableName(); \
        return loaded; \
    } \
    variableType get_##framework##_##variableName(); \
    variableType get_##framework##_##variableName() \
    { \
        return constant##framework##variableName; \
    } \
    }

#define SOFT_LINK_FUNCTION_FOR_HEADER(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    extern resultType (*softLink##framework##functionName) parameterDeclarations; \
    inline resultType softLink_##framework##_##functionName parameterDeclarations \
    { \
        return softLink##framework##functionName parameterNames; \
    } \
    } \
    inline __attribute__((__always_inline__)) resultType functionName parameterDeclarations \
    {\
        return functionNamespace::softLink##framework##functionName parameterNames; \
    }

#define SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames, export) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    static resultType init##framework##functionName parameterDeclarations; \
    export resultType(*softLink##framework##functionName) parameterDeclarations = init##framework##functionName; \
    static resultType init##framework##functionName parameterDeclarations \
    { \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            softLink##framework##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
            RELEASE_ASSERT_WITH_MESSAGE(softLink##framework##functionName, "%s", dlerror()); \
        }); \
        return softLink##framework##functionName parameterNames; \
    } \
    }

#define SOFT_LINK_FUNCTION_FOR_SOURCE(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames, )

#define SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    extern resultType (*softLink##framework##functionName) parameterDeclarations; \
    bool canLoad_##framework##_##functionName(); \
    bool init_##framework##_##functionName(); \
    resultType softLink_##framework##_##functionName parameterDeclarations; \
    }

#define SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(functionNamespace, framework, functionName, resultType, parameterDeclarations, parameterNames) \
    WTF_EXTERN_C_BEGIN \
    resultType functionName parameterDeclarations; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    resultType (*softLink##framework##functionName) parameterDeclarations = 0; \
    bool init_##framework##_##functionName(); \
    bool init_##framework##_##functionName() \
    { \
        ASSERT(!softLink##framework##functionName); \
        softLink##framework##functionName = (resultType (*) parameterDeclarations) dlsym(framework##Library(), #functionName); \
        return !!softLink##framework##functionName; \
    } \
    \
    bool canLoad_##framework##_##functionName(); \
    bool canLoad_##framework##_##functionName() \
    { \
        static bool loaded = init_##framework##_##functionName(); \
        return loaded; \
    } \
    \
    resultType softLink_##framework##_##functionName parameterDeclarations; \
    resultType softLink_##framework##_##functionName parameterDeclarations \
    { \
        ASSERT(softLink##framework##functionName); \
        return softLink##framework##functionName parameterNames; \
    } \
    }

#define SOFT_LINK_POINTER_FOR_HEADER(functionNamespace, framework, variableName, variableType) \
    namespace functionNamespace { \
    extern variableType (*get_##framework##_##variableName)(); \
    }

#define SOFT_LINK_POINTER_FOR_SOURCE(functionNamespace, framework, variableName, variableType) \
    namespace functionNamespace { \
    static variableType init##framework##variableName(); \
    variableType (*get_##framework##_##variableName)() = init##framework##variableName; \
    static variableType pointer##framework##variableName; \
    \
    static variableType pointer##framework##variableName##Function() \
    { \
        return pointer##framework##variableName; \
    } \
    \
    static variableType init##framework##variableName() \
    { \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            void** pointer = static_cast<void**>(dlsym(framework##Library(), #variableName)); \
            RELEASE_ASSERT_WITH_MESSAGE(pointer, "%s", dlerror()); \
            pointer##framework##variableName = static_cast<variableType>(*pointer); \
            get_##framework##_##variableName = pointer##framework##variableName##Function; \
        }); \
        return pointer##framework##variableName; \
    } \
    }

#define SOFT_LINK_VARIABLE_FOR_HEADER(functionNamespace, framework, variableName, variableType) \
    WTF_EXTERN_C_BEGIN \
    extern variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    variableType * get_##framework##_##variableName(); \
    }

#define SOFT_LINK_VARIABLE_FOR_SOURCE(functionNamespace, framework, variableName, variableType) \
    WTF_EXTERN_C_BEGIN \
    extern variableType variableName; \
    WTF_EXTERN_C_END \
    namespace functionNamespace { \
    variableType * get_##framework##_##variableName(); \
    variableType * get_##framework##_##variableName() \
    { \
        static variableType * variable##framework##variableName; \
        static dispatch_once_t once; \
        dispatch_once(&once, ^{ \
            void* variable = dlsym(framework##Library(), #variableName); \
            RELEASE_ASSERT_WITH_MESSAGE(variable, "%s", dlerror()); \
            variable##framework##variableName = static_cast<variableType *>(variable); \
        }); \
        return variable##framework##variableName; \
    } \
    }

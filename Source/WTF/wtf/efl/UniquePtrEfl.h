/*
 * Copyright (C) 2014 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UniquePtrEfl_h
#define UniquePtrEfl_h

#if PLATFORM(EFL)

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_IMF.h>
#include <Eina.h>
#include <Evas.h>
#include <Evas_GL.h>

namespace WTF {

template<typename T> struct EflPtrDeleter {
    void operator()(T* ptr) const = delete;
};

template<typename T>
using EflUniquePtr = std::unique_ptr<T, EflPtrDeleter<T>>;

#define FOR_EACH_EFL_DELETER(macro) \
    macro(Ecore_Evas, ecore_evas_free) \
    macro(Ecore_IMF_Context, ecore_imf_context_del) \
    macro(Ecore_Pipe, ecore_pipe_del) \
    macro(Eina_Hash, eina_hash_free) \
    macro(Eina_Module, eina_module_free) \
    macro(Evas_Object, evas_object_del) \
    macro(Evas_GL, evas_gl_free)

#define WTF_DEFINE_EFLPTR_DELETER(typeName, deleterFunc) \
    template<> struct EflPtrDeleter<typeName> \
    { \
        void operator() (typeName* ptr) const \
        { \
            if (ptr) \
                deleterFunc(ptr); \
        } \
    };

FOR_EACH_EFL_DELETER(WTF_DEFINE_EFLPTR_DELETER)
#undef FOR_EACH_EFL_DELETER

} // namespace WTF

using WTF::EflUniquePtr;

#endif // PLATFORM(EFL)

#endif // UniquePtrEfl_h

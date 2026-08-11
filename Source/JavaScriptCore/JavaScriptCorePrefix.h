/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if defined(__APPLE__)
#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif
#endif

#include <ctype.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__)
#include <strings.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#ifdef __cplusplus
#include "B3ValueRep.h"
#include "ButterflyInlinesLight.h"
#include "Bytecodes.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "GCSegmentedArrayInlines.h"
#include "GenericArgumentsImplInlines.h"
#include "Heap.h"
#include "IsoCellSetInlines.h"
#include "JSArray.h"
#include "JSArrayBufferView.h"
#include "JSCJSValue.h"
#include "JSCJSValueInlines.h"
#include "JSCJSValuePropertyInlines.h"
#include "JSDataView.h"
#include "JSGlobalObject.h"
#include "JSLexicalEnvironment.h"
#include "JSObject.h"
#include "JSObjectInlines.h"
#include "JSObjectRef.h"
#include "JSString.h"
#include "JSStringInlines.h"
#include "MarkedBlockInlines.h"
#include "Operations.h"
#include "OperationsInlines.h"
#include "OperandsInlines.h"
#include "OptionsList.h"
#include "Strong.h"
#include "Structure.h"
#include "StructureChain.h"
#include "StructureInlinesLight.h"
#include "UnlinkedMetadataTableInlines.h"
#include "VM.h"
#include "WeakGCMapInlines.h"
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/SIMDHelpers.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ValidatedReinterpretCast.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(ASSEMBLER)
#include "GPRInfo.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"
#endif

#if ENABLE(JIT)
#include "AssemblyHelpers.h"
#include "CCallHelpers.h"
#include "JITOperations.h"
#endif

#if ENABLE(WEBASSEMBLY)
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WebAssemblyFunction.h"
#endif
#endif

#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif

/* When C++ exceptions are disabled, the C++ library defines |try| and |catch|
 * to allow C++ code that expects exceptions to build. These definitions
 * interfere with Objective-C++ uses of Objective-C exception handlers, which
 * use |@try| and |@catch|. As a workaround, undefine these macros. */
#ifdef __OBJC__
#undef try
#undef catch
#endif

/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebCoreBuiltinNames_h
#define WebCoreBuiltinNames_h

#include <builtins/BuiltinUtils.h>

namespace WebCore {

#define WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro)\
    macro(state) \
    macro(underlyingSource) \
    macro(queue) \
    macro(queueSize) \
    macro(started) \
    macro(closeRequested) \
    macro(pullAgain) \
    macro(pulling) \
    macro(reader) \
    macro(storedError) \
    macro(controller) \
    macro(strategySize) \
    macro(highWaterMark) \
    macro(readRequests) \
    macro(ownerReadableStream) \
    macro(closedPromise) \
    macro(closedPromiseResolve) \
    macro(closedPromiseReject) \
    macro(controlledReadableStream) \
    macro(readableStreamClosed) \
    macro(readableStreamReadable) \
    macro(readableStreamErrored) \
    macro(ReadableStreamReader) \
    macro(ReadableStreamController) \

class WebCoreBuiltinNames {
public:
    explicit WebCoreBuiltinNames(JSC::VM* vm)
        : m_vm(*vm)
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_NAMES)
    {
#define EXPORT_NAME(name) m_vm.propertyNames->appendExternalName(name##PublicName(), name##PrivateName());
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(EXPORT_NAME)
#undef EXPORT_NAME
    }

    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR)

private:
    JSC::VM& m_vm;
    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES)
};

} // namespace WebCore

#endif // WebCoreBuiltinNames_h

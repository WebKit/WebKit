/*
 *  Copyright (C) 2015 Canon Inc. All rights reserved.
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

#ifndef JSReadableStreamPrivateConstructors_h
#define JSReadableStreamPrivateConstructors_h

#if ENABLE(STREAMS_API)

namespace JSC {

struct ClassInfo;
class JSObject;
class VM;

}

namespace WebCore {

class JSDOMGlobalObject;

JSC::JSObject* createReadableStreamControllerPrivateConstructor(JSC::VM&, JSDOMGlobalObject&);
JSC::JSObject* createReadableStreamReaderPrivateConstructor(JSC::VM&, JSDOMGlobalObject&);

} // namespace WebCore

#endif

#endif // ReadableStreamConstructors_h

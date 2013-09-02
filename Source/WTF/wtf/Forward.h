/*
 *  Copyright (C) 2006, 2009, 2011 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_Forward_h
#define WTF_Forward_h

#include <stddef.h>

namespace WTF {

    template<typename T> class Function;
    template<typename T> class OwnArrayPtr;
    template<typename T> class OwnPtr;
    template<typename T> class PassOwnArrayPtr;
    template<typename T> class PassOwnPtr;
    template<typename T> class PassRefPtr;
    template<typename T> class RefPtr;
    template<typename T> class Ref;
    template<typename T, size_t inlineCapacity, typename OverflowHandler> class Vector;

    class AtomicString;
    class AtomicStringImpl;
    class BinarySemaphore;
    class CString;
    class Decoder;
    class Encoder;
    class FunctionDispatcher;
    class PrintStream;
    class String;
    template <typename T> class StringBuffer;
    class StringBuilder;
    class StringImpl;
}

using WTF::Function;
using WTF::OwnArrayPtr;
using WTF::OwnPtr;
using WTF::PassOwnArrayPtr;
using WTF::PassOwnPtr;
using WTF::PassRefPtr;
using WTF::RefPtr;
using WTF::Ref;
using WTF::Vector;

using WTF::AtomicString;
using WTF::AtomicStringImpl;
using WTF::BinarySemaphore;
using WTF::CString;
using WTF::Encoder;
using WTF::Decoder;
using WTF::FunctionDispatcher;
using WTF::PrintStream;
using WTF::String;
using WTF::StringBuffer;
using WTF::StringBuilder;
using WTF::StringImpl;

#endif // WTF_Forward_h

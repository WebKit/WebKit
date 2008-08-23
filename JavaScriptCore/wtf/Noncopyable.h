/*
 *  Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef WTF_Noncopyable_h
#define WTF_Noncopyable_h

// We don't want argument-dependent lookup to pull in everything from the WTF
// namespace when you use Noncopyable, so put it in its own namespace.

namespace WTFNoncopyable {

    class Noncopyable {
        Noncopyable(const Noncopyable&);
        Noncopyable& operator=(const Noncopyable&);
    protected:
        Noncopyable() { }
        ~Noncopyable() { }
    };

} // namespace WTFNoncopyable

using WTFNoncopyable::Noncopyable;

#endif // WTF_Noncopyable_h

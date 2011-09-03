/*
 *  Copyright (C) 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef WeakReferenceHarvester_h
#define WeakReferenceHarvester_h

#include <stdint.h>

namespace JSC {

class MarkStack;
class SlotVisitor;

class WeakReferenceHarvester {
public:
    virtual void visitWeakReferences(SlotVisitor&) = 0;
    
protected:
    WeakReferenceHarvester()
        : m_nextAndFlag(0)
    {
    }
    
    virtual ~WeakReferenceHarvester() { }

private:
    friend class MarkStack;
    friend class SlotVisitor;
    
    uintptr_t m_nextAndFlag;
};

} // namespace JSC

#endif // WeakReferenceHarvester_h

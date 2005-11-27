// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KJS_INTERPRETER_LOCK_H
#define KJS_INTERPRETER_LOCK_H

namespace KJS {

    // to make it safe to use JavaScript on multiple threads, it is
    // important to lock before doing anything that allocates a
    // garbage-collected object or which may affect other shared state
    // such as the protect count hash table. The simplest way to do
    // this is by having a local JSLock object for the scope
    // where the lock must be held. The lock is recursive so nesting
    // is ok.

    // Sometimes it is necessary to temporarily release the lock -
    // since it is recursive you have to actually release all locks
    // held by your thread. This is safe to do if you are executing
    // code that doesn't require the lock, and reacquire the right
    // number of locks at the end. You can do this by constructing a
    // locally scoped JSLock::DropAllLocks object.

    class JSLock
    {
    public:
        JSLock() 
        {
            lock();
        }
        ~JSLock() { 
            unlock(); 
        }
        
        static void lock();
        static void unlock();
        static int lockCount();
        
        class DropAllLocks {
        public:
            DropAllLocks();
            ~DropAllLocks();
        private:
            int m_lockCount;
            
            DropAllLocks(const DropAllLocks&);
            DropAllLocks& operator=(const DropAllLocks&);
        };
        
    private:
        JSLock(const JSLock&);
        JSLock& operator=(const JSLock&);
    };

} // namespace

#endif // KJS_INTERPRETER_LOCK_H

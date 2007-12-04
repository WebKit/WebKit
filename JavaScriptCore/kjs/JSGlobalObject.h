// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2007 Apple Ince. All rights reserved.
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

#ifndef KJS_GlobalObject_h
#define KJS_GlobalObject_h

#include "object.h"

namespace KJS {

    class Interpreter;

    class JSGlobalObject : public JSObject {
    public:
        JSGlobalObject() { }
        JSGlobalObject(JSValue* proto) : JSObject(proto) { }

        Interpreter* interpreter() const { return m_interpreter.get(); }
        void setInterpreter(std::auto_ptr<Interpreter> i) { m_interpreter = i; }

        virtual void mark();

        virtual bool isGlobalObject() const { return true; }

        virtual ExecState* globalExec();

        virtual bool shouldInterruptScript() const { return true; }

        virtual bool isSafeScript(const JSGlobalObject*) const { return true; }

    private:
        std::auto_ptr<Interpreter> m_interpreter;
    };

} // namespace KJS

#endif // KJS_GlobalObject_h

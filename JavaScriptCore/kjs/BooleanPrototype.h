/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef BooleanPrototype_h
#define BooleanPrototype_h

#include "BooleanObject.h"

namespace KJS {

    class FunctionPrototype;
    class ObjectPrototype;

    /**
     * @internal
     *
     * The initial value of Boolean.prototype (and thus all objects created
     * with the Boolean constructor
     */
    class BooleanPrototype : public BooleanObject {
    public:
        BooleanPrototype(ExecState*, ObjectPrototype*, FunctionPrototype*);
    };

} // namespace KJS

#endif // BooleanPrototype_h

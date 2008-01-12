/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef Activation_h
#define Activation_h

#include "ExecState.h"
#include "JSVariableObject.h"
#include "object.h"

namespace KJS {

    class Arguments;
    class FunctionImp;

    class ActivationImp : public JSVariableObject {
        friend class JSGlobalObject;
        friend struct StackActivation;
    private:
        struct ActivationData : public JSVariableObjectData {
            ActivationData() : isOnStack(true), leftRelic(false) { }
            ActivationData(const ActivationData&);

            ExecState* exec;
            FunctionImp* function;
            Arguments* argumentsObject;

            bool isOnStack : 1;
            bool leftRelic : 1;
        };

    public:
        ActivationImp() { }
        ActivationImp(const ActivationData&, bool);

        virtual ~ActivationImp();

        void init(ExecState*);

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier&, JSValue*, int attr = None);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        virtual void mark();
        void markChildren();

        virtual bool isActivationObject() { return true; }
    
        bool isOnStack() const { return d()->isOnStack; }
        bool needsPop() const { return d()->isOnStack || d()->leftRelic; }

    private:
        static PropertySlot::GetValueFunc getArgumentsGetter();
        static JSValue* argumentsGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot&);
        void createArgumentsObject(ExecState*);
        ActivationData* d() const { return static_cast<ActivationData*>(JSVariableObject::d); }
    };

    const size_t activationStackNodeSize = 32;

    struct StackActivation {
        StackActivation() { activationStorage.JSVariableObject::d = &activationDataStorage; }
        StackActivation(const StackActivation&);
      
        ActivationImp activationStorage;
        ActivationImp::ActivationData activationDataStorage;
    };

    struct ActivationStackNode {
        ActivationStackNode* prev;
        StackActivation data[activationStackNodeSize];
    };

} // namespace

#endif

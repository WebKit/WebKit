/*
    Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2016 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSSVGLength.h"

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "SVGAnimatedProperty.h"
#include "SVGException.h"
#include "SVGLengthContext.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSValue JSSVGLength::value(ExecState& state) const
{
    SVGLength& podImp = wrapped().propertyReference();
    ExceptionCode ec = 0;
    SVGLengthContext lengthContext(wrapped().contextElement());
    float value = podImp.value(lengthContext, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return jsNumber(value);
}

void JSSVGLength::setValue(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (wrapped().isReadOnly()) {
        setDOMException(&state, NO_MODIFICATION_ALLOWED_ERR);
        return;
    }

    if (!value.isUndefinedOrNull() && !value.isNumber() && !value.isBoolean()) {
        throwVMTypeError(&state, scope);
        return;
    }

    SVGLength& podImp = wrapped().propertyReference();

    ExceptionCode ec = 0;
    SVGLengthContext lengthContext(wrapped().contextElement());
    podImp.setValue(value.toFloat(&state), lengthContext, ec);
    if (ec) {
        setDOMException(&state, ec);
        return;
    }

    wrapped().commitChange();
}

JSValue JSSVGLength::convertToSpecifiedUnits(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (wrapped().isReadOnly()) {
        setDOMException(&state, NO_MODIFICATION_ALLOWED_ERR);
        return jsUndefined();
    }

    SVGLength& podImp = wrapped().propertyReference();

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    unsigned short unitType = state.uncheckedArgument(0).toUInt32(&state);
    if (state.hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    SVGLengthContext lengthContext(wrapped().contextElement());
    podImp.convertToSpecifiedUnits(unitType, lengthContext, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    wrapped().commitChange();
    return jsUndefined();
}

}

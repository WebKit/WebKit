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
#include "JSDOMConvert.h"
#include "SVGAnimatedProperty.h"
#include "SVGException.h"
#include "SVGLengthContext.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSValue JSSVGLength::value(ExecState& state) const
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return toJS<IDLUnrestrictedFloat>(state, scope, wrapped().propertyReference().valueForBindings(SVGLengthContext { wrapped().contextElement() }));
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

    auto floatValue = value.toFloat(&state);
    RETURN_IF_EXCEPTION(scope, void());

    auto result = wrapped().propertyReference().setValue(floatValue, SVGLengthContext { wrapped().contextElement() });
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
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
        return { };
    }

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    unsigned short unitType = state.uncheckedArgument(0).toUInt32(&state);
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto result = wrapped().propertyReference().convertToSpecifiedUnits(unitType, SVGLengthContext { wrapped().contextElement() });
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    wrapped().commitChange();
    return jsUndefined();
}

}

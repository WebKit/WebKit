/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Wrap a value at the boundary between the incubating realm and `shadowRealm`:
// if `fromShadowRealm` is false, we are wrapping an object from the incubating
// realm; if true, we are wrapping an object from the shadow realm
@linkTimeConstant
function wrapRemoteValue(fromShadowRealm, shadowRealm, target)
{
    "use strict";

    if (@isCallable(target))
        return @createRemoteFunction(target, fromShadowRealm ? null : shadowRealm);

    if (@isObject(target))
        @throwTypeError("value passing between realms must be callable or primitive");

    return target;
}

function evaluate(sourceText)
{
    "use strict";

    if (!@isShadowRealm(this))
        @throwTypeError("`%ShadowRealm%.evaluate requires that |this| be a ShadowRealm instance");

    if (typeof sourceText !== 'string')
        @throwTypeError("`%ShadowRealm%.evaluate requires that the |sourceText| argument be a string");

    return @wrapRemoteValue(true, this, @evalInRealm(this, sourceText));
}

@linkTimeConstant
function crossRealmThrow(error)
{
    "use strict";

    // re-throw because import issues raise errors using the realm's global object
    @throwTypeError(@toString(error));
}

function importValue(specifier, exportName)
{
    "use strict";

    if (!@isShadowRealm(this))
        @throwTypeError("`%ShadowRealm%.importValue requires that |this| be a ShadowRealm instance");

    var specifierString = @toString(specifier);

    if (typeof exportName !== 'string')
        @throwTypeError("`%ShadowRealm%.importValue requires that the |exportName| argument be a string");

    var lookupBinding = (module) => {
        var lookup = module[exportName]
        if (lookup === @undefined)
            @throwTypeError("%ShadowRealm%.importValue requires |exportName| to exist in the |specifier|");

        return @wrapRemoteValue(true, this, lookup);
    };

    return @importInRealm(this, specifierString).@then(lookupBinding, @crossRealmThrow);
}

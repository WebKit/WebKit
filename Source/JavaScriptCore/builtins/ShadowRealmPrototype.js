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

@globalPrivate
function wrap(target)
{
    "use strict";

    if (@isCallable(target)) {
        var wrapped = function(/* args... */) {
            var length = arguments.length;
            var wrappedArgs = @newArrayWithSize(length);
            for (var index = 0; index < length; ++index)
                @putByValDirect(wrappedArgs, index, @wrap(arguments[index]));

            var result = target.@apply(@undefined, wrappedArgs);
            return @wrap(result);
        };
        delete wrapped['name'];
        delete wrapped['length'];
        return wrapped;
    } else if (@isObject(target)) {
        @throwTypeError("value passing between realms must be callable or primitive");
    }
    return target;
}

function evaluate(sourceText)
{
    "use strict";

    if (!@isShadowRealm(this))
        @throwTypeError("`%ShadowRealm%.evaluate requires that |this| be a ShadowRealm instance");

    if (typeof sourceText !== 'string')
        @throwTypeError("`%ShadowRealm%.evaluate requires that the |sourceText| argument be a string");

    var result = @evalInRealm(this, sourceText)
    return @wrap(result);
}

function importValue(specifier, exportName)
{
    "use strict";

    if (!@isShadowRealm(this))
        @throwTypeError("`%ShadowRealm%.importValue requires that |this| be a ShadowRealm instance");

    var exportNameString = @toString(exportName);
    var specifierString = @toString(specifier);

    var lookupBinding = (module) => {
        var lookup = module[exportNameString]
        if (lookup === @undefined)
            @throwTypeError("%ShadowRealm%.importValue requires |exportName| to exist in the |specifier|");

        return @wrap(lookup);
    };

    var crossRealmThrow = (error) => {
        // re-throw because import issues raise errors using the realm's global object
        @throwTypeError(@toString(error));
    };

    return @importInRealm(this, specifierString).@then(lookupBinding, crossRealmThrow);
}

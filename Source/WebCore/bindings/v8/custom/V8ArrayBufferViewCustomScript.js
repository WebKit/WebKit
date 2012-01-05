/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

(function() {

var optimizeSetMethod = function(type)
{
    if (typeof type === 'function' &&
        typeof type.prototype !== 'undefined' && 
        typeof type.prototype.set === 'function') {
        type.prototype.set = (function() {
            var nativeSet = type.prototype.set;
            var f = function(source, offset)
            {
                if (source.constructor === Array) {
                    var length = source.length;
                    offset = offset || 0;
                    if (offset < 0 || offset + length > this.length) {
                        return nativeSet.call(this, source, offset);
                    }
                    for (var i = 0; i < length; i++)
                        this[i + offset] = source[i];
                } else
                    return nativeSet.call(this, source, offset);
            }
            f.name = "set";
            return f;
        })();
    }
};

optimizeSetMethod(Float32Array);
optimizeSetMethod(Float64Array);
optimizeSetMethod(Int8Array);
optimizeSetMethod(Int16Array);
optimizeSetMethod(Int32Array);
optimizeSetMethod(Uint8Array);
optimizeSetMethod(Uint16Array);
optimizeSetMethod(Uint32Array);

})();


/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#ifndef GeneratorThisMode_h
#define GeneratorThisMode_h

namespace JSC {

// http://ecma-international.org/ecma-262/6.0/#sec-functionallocate
// http://ecma-international.org/ecma-262/6.0/#sec-ecmascript-function-objects-construct-argumentslist-newtarget
// When the function is a GeneratorFunction, its [[ConstructorKind]] is always "derived".
// This means that OrdinaryCallBindThis in section 9.2.2 is never executed for GeneratorFunction.
// So, when we execute the following,
//
//     function *gen()
//     {
//        yield this
//     }
//
//     {
//         let g = gen();
//         // |this| should be global.
//         g.next();
//     }
//
//     {
//         let g = new gen();
//         // |this| in gen should be TDZ (and it won't be filled).
//         g.next();
//     }
//
// The following flag manages this state. When GeneratorFunction is called as a constructor, it returns a Generator that function has GeneratorThisMode::Empty flag.
// In this case, when accessing |this|, a TDZ reference error occurs.
enum class GeneratorThisMode : unsigned {
    Empty,
    NonEmpty
};

}

#endif // GeneratorThisMode_h

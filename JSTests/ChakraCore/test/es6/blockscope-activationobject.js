//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Make sure space is allocated on block scope object for delay-captured lexical vars.

(function (arg) {
    let a0 = 0;
    let a1 = 0;
    let a2 = 0;
    let a3 = 0
    let a4 = 0;
    let a5 = 0;
    let a6 = 0;
    let a7 = 0;

    class class15 {
        constructor() {
            a0;
            a1;
            a2;
            a3;
            a4;
            a5;
            a6;
            a7;
        }
    }

    arguments[0];
})();

WScript.Echo('pass');
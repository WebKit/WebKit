//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj1 = {};
var func2 = function () {
    function v1() {
        arguments;
    }
    for (var v4 = 0; v4 < 2; v4++) {
        v1();
        function v5() {
            if (v4 < 10) {
                return;
            }
            for (; ; __loopvar6) {
            }
        }
        function v8() {
            return v5();
        }
        function v9() {
            return v8();
        }
        v9();
    }
};
obj1.method0 = func2;
(function () {
})(obj1.method0());

WScript.Echo('pass');
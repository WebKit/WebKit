//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test TDZ behavior when writing to let declared in switch and closure-captured.
function test0() {
    switch (x) {
        case 1:
            let inner;
            inner = 2;
            function f() { if (inner !== 2) WScript.Echo('fail'); }
            f();
            break;
        case 2:
        case 0:
            try {
                inner = 1;
            }
            catch (e) {
                break;
            }
            WScript.Echo('fail');
    }
}
var x = 0;
test0();
x = 1;
test0();

function test1() {
    switch (x) {
        case 1:
            let inner;
            inner = 2;
            function f() { if (eval('inner') !== 2) WScript.Echo('fail'); }
            f();
            break;
        case 2:
        case 0:
            try {
                inner = 1;
            }
            catch (e) {
                break;
            }
            WScript.Echo('fail');
    }
}
var x = 0;
test1();
x = 1;
test1();

// GitHub issue #505: Use before declaration emitted to early and
// prevented Emit visitation of AST subtrees
function test2() {
    var calledg = false;
    function g() { calledg = true; }
    function f() {
        var k = 1;
        d = g();
        let d;
    }

    try {
        f();
    } catch (e) {
        if (!calledg) {
            WScript.Echo('test2 failed to call g() in f()');
        }
        return;
    }
    WScript.Echo('test2 failed to throw TDZ error in f()');
}
test2();

function test3() {
    // this used to assert before #505 was fixed; arguments expression
    // was not visited by Emit() and did not get a location for its sym
    // properly assigned
    function f()
    {
        var m = 1;
        (() => {
            m = k1 = arguments;
            let k1 = 10;
        })();
    };

    try {
        f();
    } catch (e) {
        return;
    }
    WScript.Echo('fail');
}
test3();

function test4() {
    // this case come from jsfunfuzz and is the smallest I could
    // get the repro to be
    var window = function() { return this; };

    {
        var z = w, u3056 = this, w;
    }

    if (w !== undefined) {
        WScript.Echo('test4 failed, w not undefined first time');
    }

    function f() {
        var window = function() { return this; };
        //eval('');
        try {
            u3056( "" ) = z = new Object();
        } catch(e) {
            if (e.message !== 'Use before declaration') {
                WScript.Echo('test4 failed, threw unexpected error: ' + e.message);
            }
        };
        class u3056 {static b(){ var x = z; }}(window).bind;
    };
    f();

    if (w !== undefined) {
        WScript.Echo('test4 failed, w not undefined second time');
    }
}
test4();

WScript.Echo('pass');

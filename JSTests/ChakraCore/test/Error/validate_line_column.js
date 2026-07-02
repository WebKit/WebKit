//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Validating that error thrown has right line and column number

function foo(validate) {
    try {
        validate();
    } catch (e) {
        print(e.stack);
    }
}

foo(function() {
    ([z1]);         // Error thrown here.
});

foo(function() {
    ({a:z1});         // Error thrown here.
});

foo(function() {
    var a;
    a;class b extends ([]){};    // Error thrown here.
});

foo(function() {
    (typeof a.b);         // Error thrown here.
});

foo(function() {
    var k = 1;
    !a.b;         // Error thrown here.
});

foo(function() {
    var k = 1;
    ~a.b;         // Error thrown here.
});

foo(function() {
    var k = 1;
    (a.b && a.b);          // Error thrown here.
});

foo(function() {
    var k = 1;
    (a.b || a.b);         // Error thrown here.
});

foo(function() {
    var k = 1;
    (a.b * a.b);         // Error thrown here.
});

foo(function() {
    var k = 1;
    `${a.b}`;         // Error thrown here.
});


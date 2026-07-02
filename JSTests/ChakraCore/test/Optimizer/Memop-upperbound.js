//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Reduction of live site code that exposed use of bad upper bound in a memop transformation

function f(a, b) {
    var c, d = a.length < b.length ? a.length : b.length;
    for (c = 0; d > c; c++)
        a[c] = b[c];
    for (c = d; c < a.length; c++)
        a[c] = 0
}

var a = Array(3);
var b = [0, 1, 2, 3];


f(a, b);
WScript.Echo(a);
b = [0];
a = [0, 1, 2, 3];
f(a, b);
WScript.Echo(a);
a = [2, 2];
b = [8, 7, 6, 5, 2, 2, 2, 2, 2];
f(a, b);
WScript.Echo(a);

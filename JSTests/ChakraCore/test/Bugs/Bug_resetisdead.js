//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test(p1, p2, p3, p4) {
    var a = p1 + p3; var b = p2 + p4; if ((p1 + p2) > (p3 + p4) & a > b) {
        return 1;
    }

    if ((p1 + p2) > (p3 + p4) | (p1 + p3) > (p2 + p4)) {
        return 2;
    }
    return 3;
}

test(1, 20, 3, -1);
test(5, 4, 2, -2);
test(15, 3, -11, 4);

WScript.Echo("PASSED");

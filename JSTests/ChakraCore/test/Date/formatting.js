//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function CUT_NAME(str) {
    return str.replace("(PST)", "(Pacific Standard Time)")
              .replace("(PDT)", "(Pacific Daylight Time)");
}

for (var i = 0; i < 4 * 60; i++) {
    var d = new Date(2012, 2, 11, 0, i, 0);
    WScript.Echo(CUT_NAME(d.toString()));
}
for (var i = 0; i < 4 * 60; i++) {
    var d = new Date(2012, 10, 4, 0, i, 0);
    WScript.Echo(CUT_NAME(d.toString()));
}

// BLUE: 538457
var bug538457 = new Date(1383672529000000);
WScript.Echo(CUT_NAME(bug538457.toString()));

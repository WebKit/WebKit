//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var func1 = function() {
    var ary = new Array(1);
    ary.map(function(a) { });
}

func1();
print("Pass");

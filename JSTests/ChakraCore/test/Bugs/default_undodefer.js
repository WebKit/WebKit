//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Validating that function with default does not assert with ForceUndoDefer
var bar = function () { }

function foo(a = function () {} ) { 
    print("Pass");
}

foo();

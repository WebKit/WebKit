//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var globalMsg = 'Hello'

function foo(thing)
{
    var ctr = 0;
    return function () { return globalMsg + ' ' + thing + ' #' + ctr++; };
}

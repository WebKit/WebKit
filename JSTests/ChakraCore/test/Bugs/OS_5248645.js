//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//Switches: -maxinterpretCount:2 -off:simplejit -off:dynamicProfile
// Exercises IRBuilder::BuildCallIExtendedFlags()

function f() {
    eval("");
};

f();
f();
f();
print("pass");

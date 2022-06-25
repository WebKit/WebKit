//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// OS 6471427: With -forceserialized, the name of a function is not displayed correctly.

class class6 {
    static func88() {}
}

if ("func88" == class6.func88.name) {
    WScript.Echo("Pass");
} else {
    WScript.Echo("Fail");
}

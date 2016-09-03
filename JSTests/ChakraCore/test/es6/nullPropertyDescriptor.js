//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try {
    Object.defineProperty({}, "x", null);
} catch (e) {
    if (e instanceof TypeError) {
        if (e.message !== "Invalid descriptor for property 'x'") {
            print('FAIL');
        } else {
            print('PASS');
        }
    } else {
        print('FAIL');
    }
}

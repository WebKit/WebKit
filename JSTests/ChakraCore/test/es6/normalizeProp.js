//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var o = { blah: "abc" }
Object.defineProperty(o, 'hello', {
    get: function() { return 42; },
    set: function() { },
    enumerable: true
})

for (var p in o) {
    print(p.normalize())
}

print("PASS")

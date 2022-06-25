// Copyright (C) 2020 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.prototype.tostring
description: >
    Else if O has an [[ErrorData]] internal slot, let builtinTag be "Error".
---*/
assert.sameValue(new Error().toString(), 'Error', 'new Error.toString() returns "Error"');

Error.prototype.toString = Object.prototype.toString;
assert.sameValue(new Error().toString(), '[object Error]', 'new Error.toString() returns "[object Error]" (Object.prototype.toString)');


// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - module
description: |
  pending
esid: pending
---*/

var x = "ok";

export {x as "*"};

import {"*" as y} from "./module-export-name-star.js"

assert.sameValue(y, "ok");


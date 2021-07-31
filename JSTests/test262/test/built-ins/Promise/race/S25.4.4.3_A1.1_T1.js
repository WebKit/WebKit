// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: Promise.race is callable
es6id: S25.4.4.3_A1.1_T1
author: Sam Mikes
description: Promise.race is callable
---*/

if (typeof Promise.race !== "function") {
  throw new Test262Error("Expected Promise.race to be a function, actually " + typeof Promise.race);
}

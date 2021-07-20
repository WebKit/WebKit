// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.instant
description: Temporal.now.instant is extensible.
features: [Temporal]
---*/

assert(Object.isExtensible(Temporal.now.instant));

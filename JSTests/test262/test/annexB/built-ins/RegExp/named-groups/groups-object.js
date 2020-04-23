// Copyright 2017 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Properties of the groups object are created with CreateDataProperty
esid: sec-regexpbuiltinexec
features: [regexp-named-groups]
info: |
  Runtime Semantics: RegExpBuiltinExec ( R, S )
    24. If _R_ contains any |GroupName|, then
      a. Let _groups_ be ObjectCreate(*null*).
    25. Else,
      a. Let _groups_ be *undefined*.
    26. Perform ! CreateDataProperty(_A_, `"groups"`, _groups_).
---*/

// The `__proto__` property on the groups object is not special,
// and does not affect the [[Prototype]] of the resulting groups object.
let {groups} = /(?<__proto__>.)/.exec("a");
assert.sameValue("a", groups.__proto__);
assert.sameValue(null, Object.getPrototypeOf(groups));

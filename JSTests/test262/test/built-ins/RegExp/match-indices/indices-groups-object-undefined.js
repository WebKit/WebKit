// Copyright 2019 Ron Buckton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: The groups object of indices is created unconditionally.
includes: [propertyHelper.js]
esid: sec-makeindicesarray
features: [regexp-named-groups, regexp-match-indices]
info: |
  MakeIndicesArray ( S, indices, groupNames )
    8. If _groupNames_ is not *undefined*, then
      a. Let _groups_ be ! ObjectCreate(*null*).
    9. Else,
      a. Let _groups_ be *undefined*.
    10. Perform ! CreateDataProperty(_A_, `"groups"`, _groups_).
---*/


const re = /./;
const indices = re.exec("a").indices;
verifyProperty(indices, 'groups', {
  writable: true,
  enumerable: true,
  configurable: true,
  value: undefined
});

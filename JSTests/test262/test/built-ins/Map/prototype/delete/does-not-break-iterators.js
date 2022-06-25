// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-map.prototype.delete
description: >
  Deleting an entry does not break a [[MapData]] List.
info: |
  Map.prototype.delete ( key )

  4. Let entries be the List that is the value of M’s [[MapData]] internal slot.
  5. Repeat for each Record {[[key]], [[value]]} p that is an element of entries,
    a. If p.[[key]] is not empty and SameValueZero(p.[[key]], key) is true, then
      i. Set p.[[key]] to empty.
      ii. Set p.[[value]] to empty.
      iii. Return true.
  ...
---*/

var m = new Map([
  ['a', 1],
  ['b', 2],
  ['c', 3]
]);
var e = m.entries();

e.next();
m.delete('b');

var n = e.next();

assert.sameValue(n.value[0], 'c');
assert.sameValue(n.value[1], 3);

n = e.next();
assert.sameValue(n.value, undefined);
assert.sameValue(n.done, true);

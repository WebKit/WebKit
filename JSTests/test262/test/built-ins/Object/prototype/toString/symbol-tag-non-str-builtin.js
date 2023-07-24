// Copyright (C) 2019 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-object.prototype.tostring
description: >
  Non-string values of `Symbol.toStringTag` property are ignored.
info: |
  Object.prototype.toString ( )

  [...]
  15. Let tag be ? Get(O, @@toStringTag).
  16. If Type(tag) is not String, set tag to builtinTag.
  17. Return the string-concatenation of "[object ", tag, and "]".
features: [Symbol.toStringTag, Symbol.iterator, generators, WeakMap, iterator-helpers]
---*/

var toString = Object.prototype.toString;

delete Symbol.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(Symbol('desc')), '[object Object]');

Object.defineProperty(Math, Symbol.toStringTag, {value: Symbol()});
assert.sameValue(toString.call(Math), '[object Object]');

var strIter = ''[Symbol.iterator]();
var strIterProto = Object.getPrototypeOf(strIter);
assert.sameValue(toString.call(strIter), '[object String Iterator]');
delete strIterProto[Symbol.toStringTag];
assert.sameValue(toString.call(strIter), '[object Iterator]');

var arrIter = [][Symbol.iterator]();
var arrIterProto = Object.getPrototypeOf(arrIter)
assert.sameValue(toString.call(arrIter), '[object Array Iterator]');
Object.defineProperty(arrIterProto, Symbol.toStringTag, {value: null});
assert.sameValue(toString.call(arrIter), '[object Object]');

var map = new Map();
delete Map.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(map), '[object Object]');

var mapIter = map[Symbol.iterator]();
var mapIterProto = Object.getPrototypeOf(mapIter);
assert.sameValue(toString.call(mapIter), '[object Map Iterator]');
Object.defineProperty(mapIterProto, Symbol.toStringTag, {
  get: function() { return new String('ShouldNotBeUnwrapped'); },
});
assert.sameValue(toString.call(mapIter), '[object Object]');

var set = new Set();
delete Set.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(set), '[object Object]');

var setIter = set[Symbol.iterator]();
var setIterProto = Object.getPrototypeOf(setIter);
assert.sameValue(toString.call(setIter), '[object Set Iterator]');
Object.defineProperty(setIterProto, Symbol.toStringTag, {value: false});
assert.sameValue(toString.call(setIter), '[object Object]');

var wm = new WeakMap();
delete WeakMap.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(wm), '[object Object]');

var ws = new WeakSet();
Object.defineProperty(WeakSet.prototype, Symbol.toStringTag, {value: 0});
assert.sameValue(toString.call(ws), '[object Object]');

delete JSON[Symbol.toStringTag];
assert.sameValue(toString.call(JSON), '[object Object]');

var gen = (function* () {})();
var genProto = Object.getPrototypeOf(gen);
Object.defineProperty(genProto, Symbol.toStringTag, {
  get: function() { return {}; },
});
assert.sameValue(toString.call(gen), '[object Object]');

var promise = new Promise(function() {});
delete Promise.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(promise), '[object Object]');

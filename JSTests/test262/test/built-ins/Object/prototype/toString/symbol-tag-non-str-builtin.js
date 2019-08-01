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
features: [Symbol.toStringTag, Symbol.iterator, generators, WeakMap]
---*/

var toString = Object.prototype.toString;
var defaultTag = '[object Object]';

delete Symbol.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(Symbol('desc')), defaultTag);

Object.defineProperty(Math, Symbol.toStringTag, {value: Symbol()});
assert.sameValue(toString.call(Math), defaultTag);

var strIter = ''[Symbol.iterator]();
var strIterProto = Object.getPrototypeOf(strIter);
delete strIterProto[Symbol.toStringTag];
assert.sameValue(toString.call(strIter), defaultTag);

var arrIter = [][Symbol.iterator]();
var arrIterProto = Object.getPrototypeOf(arrIter)
Object.defineProperty(arrIterProto, Symbol.toStringTag, {value: null});
assert.sameValue(toString.call(arrIter), defaultTag);

var map = new Map();
delete Map.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(map), defaultTag);

var mapIter = map[Symbol.iterator]();
var mapIterProto = Object.getPrototypeOf(mapIter);
Object.defineProperty(mapIterProto, Symbol.toStringTag, {
  get: function() { return new String('ShouldNotBeUnwrapped'); },
});
assert.sameValue(toString.call(mapIter), defaultTag);

var set = new Set();
delete Set.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(set), defaultTag);

var setIter = set[Symbol.iterator]();
var setIterProto = Object.getPrototypeOf(setIter);
Object.defineProperty(setIterProto, Symbol.toStringTag, {value: false});
assert.sameValue(toString.call(setIter), defaultTag);

var wm = new WeakMap();
delete WeakMap.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(wm), defaultTag);

var ws = new WeakSet();
Object.defineProperty(WeakSet.prototype, Symbol.toStringTag, {value: 0});
assert.sameValue(toString.call(ws), defaultTag);

delete JSON[Symbol.toStringTag];
assert.sameValue(toString.call(JSON), defaultTag);

var gen = (function* () {})();
var genProto = Object.getPrototypeOf(gen);
Object.defineProperty(genProto, Symbol.toStringTag, {
  get: function() { return {}; },
});
assert.sameValue(toString.call(gen), defaultTag);

var promise = new Promise(function() {});
delete Promise.prototype[Symbol.toStringTag];
assert.sameValue(toString.call(promise), defaultTag);

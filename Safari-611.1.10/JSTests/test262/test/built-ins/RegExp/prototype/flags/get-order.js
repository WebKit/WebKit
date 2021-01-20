// Copyright (C) 2017 Aleksey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-regexp.prototype.flags
description: Gets are performed in specified order
info: |
  get RegExp.prototype.flags

  [...]
  4. Let global be ToBoolean(? Get(R, "global")).
  6. Let ignoreCase be ToBoolean(? Get(R, "ignoreCase")).
  8. Let multiline be ToBoolean(? Get(R, "multiline")).
  10. Let dotAll be ToBoolean(? Get(R, "dotAll")).
  12. Let unicode be ToBoolean(? Get(R, "unicode")).
  14. Let sticky be ToBoolean(? Get(R, "sticky")).
features: [regexp-dotall]
---*/

var calls = '';
var re = {
  get global() {
    calls += 'g';
  },
  get ignoreCase() {
    calls += 'i';
  },
  get multiline() {
    calls += 'm';
  },
  get dotAll() {
    calls += 's';
  },
  get unicode() {
    calls += 'u';
  },
  get sticky() {
    calls += 'y';
  },
};

var get = Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get;

get.call(re);
assert.sameValue(calls, 'gimsuy');

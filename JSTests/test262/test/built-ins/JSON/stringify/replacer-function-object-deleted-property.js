// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-serializejsonproperty
description: >
  Replacer function is called on properties, deleted during stringification.
info: |
  JSON.stringify ( value [ , replacer [ , space ] ] )

  [...]
  12. Return ? SerializeJSONProperty(the empty String, wrapper).

  SerializeJSONProperty ( key, holder )

  1. Let value be ? Get(holder, key).
  [...]
  3. If ReplacerFunction is not undefined, then
    a. Set value to ? Call(ReplacerFunction, holder, « key, value »).
---*/

var obj = {
  get a() {
    delete this.b;
    return 1;
  },
  b: 2,
};

var replacer = function(key, value) {
  if (key === 'b') {
    assert.sameValue(value, undefined);
    return '<replaced>';
  }

  return value;
};

assert.sameValue(
  JSON.stringify(obj, replacer),
  '{"a":1,"b":"<replaced>"}'
);

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
/* Object.freeze */

function getme() { return 42; };
function setme(x) { };

var properties = { all:       { value:1, writable:true,  configurable:true,  enumerable: true },
                   readOnly:  { value:2, writable:false, configurable:true,  enumerable: true },
                   nonConfig: { value:3, writable:true,  configurable:false, enumerable: true },
                   none:      { value:4, writable:false, configurable:false, enumerable: true },
                   getter:    { get: getme,              configurable:false, enumerable: true },
                   setter:    { set: setme,              configurable:false, enumerable: true },
                   getandset: { get: getme, set: setme,  configurable:false, enumerable: true }
                 };
var o = Object.defineProperties({}, properties);

Object.freeze(o);

function getPropertyOf(obj) {
    return function (prop) {
        return Object.getOwnPropertyDescriptor(obj, prop);
    };
};

assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'all'),
                   { value: 1, writable:false,  enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'readOnly'),
                   { value: 2, writable:false,  enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'nonConfig'),
                    { value: 3, writable:false,  enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'none'),
                    { value: 4, writable:false,  enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'getter'),
                    { get: getme, set: (void 0), enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'setter'),
                    { set: setme, get: (void 0), enumerable:true, configurable:false }),
         true);
assert.sameValue(deepEqual(Object.getOwnPropertyDescriptor(o, 'getandset'),
                    { get: getme, set: setme,    enumerable:true, configurable:false }),
         true);


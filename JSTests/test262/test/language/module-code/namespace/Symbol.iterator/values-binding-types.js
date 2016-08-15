// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
description: Iteration over exported names
info: >
    15.2.1.18 Runtime Semantics: GetModuleNamespace

    [...]
    3. If namespace is undefined, then
       a. Let exportedNames be ? module.GetExportedNames(« »).
       b. Let unambiguousNames be a new empty List.
       c. For each name that is an element of exportedNames,
          i. Let resolution be ? module.ResolveExport(name, « », « »).
          ii. If resolution is null, throw a SyntaxError exception.
          iii. If resolution is not "ambiguous", append name to
               unambiguousNames.
       d. Let namespace be ModuleNamespaceCreate(module, unambiguousNames).
    4. Return namespace.

    9.4.6.12 ModuleNamespaceCreate (module, exports)

    [...]
    7. Set M's [[Exports]] internal slot to exports.
    [...]

    26.3.2 [ @@iterator ] ( )

    [...]
    3. Let exports be the value of N's [[Exports]] internal slot.
    4. Return ! CreateListIterator(exports).

    Note: identifiers have been selected such that runtimes which do not sort
    the [[Exports]] list may still pass. A separate test is dedicated to sort
    order.
flags: [module]
features: [Symbol.iterator, let]
---*/

import * as ns from './values-binding-types.js';

export var a_local1;
var local2;
export { local2 as b_renamed };
export { a_local1 as e_indirect } from './values-binding-types.js';
export * from './values-binding-types_.js';

var iter = ns[Symbol.iterator]();
var result;

result = iter.next();
assert.sameValue(result.done, false, 'not initially done');
assert.sameValue(result.value, 'a_local1');

result = iter.next();
assert.sameValue(result.value, 'b_renamed');
assert.sameValue(result.done, false , 'not done after "a_local1"');

result = iter.next();
assert.sameValue(result.value, 'c_localUninit1');
assert.sameValue(result.done, false, 'not done after "b_renamed"');

result = iter.next();
assert.sameValue(result.value, 'd_renamedUninit');
assert.sameValue(result.done, false, 'not done after "c_localUninit1"');

result = iter.next();
assert.sameValue(result.value, 'default');
assert.sameValue(result.done, false, 'not done after "d_renamedUninit"');

result = iter.next();
assert.sameValue(result.value, 'e_indirect');
assert.sameValue(result.done, false, 'not done after "default"');

result = iter.next();
assert.sameValue(result.value, 'f_indirectUninit');
assert.sameValue(result.done, false, 'not done after "e_indirect"');

result = iter.next();
assert.sameValue(result.value, 'g_star');
assert.sameValue(result.done, false, 'not done after "f_indirectUninit"');

result = iter.next();
assert.sameValue(result.value, 'h_starRenamed');
assert.sameValue(result.done, false, 'not done after "g_star"');

result = iter.next();
assert.sameValue(result.value, 'i_starIndirect');
assert.sameValue(result.done, false, 'not done after "h_starRenamed"');

result = iter.next();
assert.sameValue(result.done, true, 'done after "i_starIndirect"');
assert.sameValue(result.value, undefined);

result = iter.next();
assert.sameValue(result.done, true, 'done after exhaustion');
assert.sameValue(result.value, undefined);

export let c_localUninit1;
let localUninit2;
export { localUninit2 as d_renamedUninit };
export { c_localUninit1 as f_indirectUninit } from './values-binding-types.js';
export default null;

// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
description: Iteration order over exported names
info: >
    Iteration should include all non-ambiguous export names ordered as if an
    Array of those String values had been sorted using `Array.prototype.sort`
    using SortCompare as *comparefunction*.

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
flags: [module]
features: [Symbol.iterator]
---*/

var x;
export { x as π }; // u03c0
export { x as az };
export { x as __ };
export { x as za };
export { x as Z };
export { x as \u03bc };
export { x as z };
export { x as zz };
export { x as a };
export { x as A };
export { x as aa };
export { x as λ }; // u03bb
export { x as _ };
export { x as $$ };
export { x as $ };
export default null;

import * as ns from './values-order.js';

var iter = ns[Symbol.iterator]();
var result;

result = iter.next();
assert.sameValue(result.done, false, 'not initially done');
assert.sameValue(result.value, '$');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "$"');
assert.sameValue(result.value, '$$');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "$$"');
assert.sameValue(result.value, 'A');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "A"');
assert.sameValue(result.value, 'Z');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "Z"');
assert.sameValue(result.value, '_');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "_"');
assert.sameValue(result.value, '__');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "__"');
assert.sameValue(result.value, 'a');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "a"');
assert.sameValue(result.value, 'aa');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "aa"');
assert.sameValue(result.value, 'az');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "az"');
assert.sameValue(result.value, 'default');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "default"');
assert.sameValue(result.value, 'z');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "z"');
assert.sameValue(result.value, 'za');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "za"');
assert.sameValue(result.value, 'zz');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "zz"');
assert.sameValue(result.value, '\u03bb');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "\u03bb"');
assert.sameValue(result.value, '\u03bc');

result = iter.next();
assert.sameValue(result.done, false, 'not done following "\u03bc"');
assert.sameValue(result.value, '\u03c0');

result = iter.next();
assert.sameValue(result.done, true, 'done following "\u03c0"');
assert.sameValue(result.value, undefined);

result = iter.next();
assert.sameValue(result.done, true, 'done following exhaustion');
assert.sameValue(result.value, undefined);

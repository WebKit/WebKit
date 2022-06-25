// This file was procedurally generated from the following sources:
// - src/dynamic-import/ns-own-property-keys-sort.case
// - src/dynamic-import/namespace/promise.template
/*---
description: The [[OwnPropertyKeys]] internal method reflects the sorted order (value from promise then)
esid: sec-finishdynamicimport
features: [Symbol.toStringTag, dynamic-import]
flags: [generated, async]
info: |
    Runtime Semantics: FinishDynamicImport ( referencingScriptOrModule, specifier, promiseCapability, completion )

        1. If completion is an abrupt completion, ...
        2. Otherwise,
            ...
            d. Let namespace be GetModuleNamespace(moduleRecord).
            e. If namespace is an abrupt completion, perform ! Call(promiseCapability.[[Reject]], undefined, « namespace.[[Value]] »).
            f. Otherwise, perform ! Call(promiseCapability.[[Resolve]], undefined, « namespace.[[Value]] »).

    Runtime Semantics: GetModuleNamespace ( module )

        ...
        3. Let namespace be module.[[Namespace]].
        4. If namespace is undefined, then
            a. Let exportedNames be ? module.GetExportedNames(« »).
            b. Let unambiguousNames be a new empty List.
            c. For each name that is an element of exportedNames, do
                i. Let resolution be ? module.ResolveExport(name, « »).
                ii. If resolution is a ResolvedBinding Record, append name to unambiguousNames.
            d. Set namespace to ModuleNamespaceCreate(module, unambiguousNames).
        5. Return namespace.

    ModuleNamespaceCreate ( module, exports )

        ...
        4. Let M be a newly created object.
        5. Set M's essential internal methods to the definitions specified in 9.4.6.
        7. Let sortedExports be a new List containing the same values as the list exports where the
        values are ordered as if an Array of the same values had been sorted using Array.prototype.sort
        using undefined as comparefn.
        8. Set M.[[Exports]] to sortedExports.
        9. Create own properties of M corresponding to the definitions in 26.3.
        10. Set module.[[Namespace]] to M.
        11. Return M.

    26.3 Module Namespace Objects

        A Module Namespace Object is a module namespace exotic object that provides runtime
        property-based access to a module's exported bindings. There is no constructor function for
        Module Namespace Objects. Instead, such an object is created for each module that is imported
        by an ImportDeclaration that includes a NameSpaceImport.

        In addition to the properties specified in 9.4.6 each Module Namespace Object has the
        following own property:

    26.3.1 @@toStringTag

        The initial value of the @@toStringTag property is the String value "Module".

        This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.

    Module Namespace Exotic Objects

        A module namespace object is an exotic object that exposes the bindings exported from an
        ECMAScript Module (See 15.2.3). There is a one-to-one correspondence between the String-keyed
        own properties of a module namespace exotic object and the binding names exported by the
        Module. The exported bindings include any bindings that are indirectly exported using export *
        export items. Each String-valued own property key is the StringValue of the corresponding
        exported binding name. These are the only String-keyed properties of a module namespace exotic
        object. Each such property has the attributes { [[Writable]]: true, [[Enumerable]]: true,
        [[Configurable]]: false }. Module namespace objects are not extensible.


    1. Let exports be a copy of the value of O's [[Exports]] internal slot.
    2. Let symbolKeys be ! OrdinaryOwnPropertyKeys(O).
    3. Append all the entries of symbolKeys to the end of exports.
    4. Return exports.

---*/

import('./own-keys-sort_FIXTURE.js').then(ns => {

    var stringKeys = Object.getOwnPropertyNames(ns);

    assert.sameValue(stringKeys.length, 16);
    assert.sameValue(stringKeys[0], '$', 'stringKeys[0] === "$"');
    assert.sameValue(stringKeys[1], '$$', 'stringKeys[1] === "$$"');
    assert.sameValue(stringKeys[2], 'A', 'stringKeys[2] === "A"');
    assert.sameValue(stringKeys[3], 'Z', 'stringKeys[3] === "Z"');
    assert.sameValue(stringKeys[4], '_', 'stringKeys[4] === "_"');
    assert.sameValue(stringKeys[5], '__', 'stringKeys[5] === "__"');
    assert.sameValue(stringKeys[6], 'a', 'stringKeys[6] === "a"');
    assert.sameValue(stringKeys[7], 'aa', 'stringKeys[7] === "aa"');
    assert.sameValue(stringKeys[8], 'az', 'stringKeys[8] === "az"');
    assert.sameValue(stringKeys[9], 'default', 'stringKeys[9] === "default"');
    assert.sameValue(stringKeys[10], 'z', 'stringKeys[10] === "z"');
    assert.sameValue(stringKeys[11], 'za', 'stringKeys[11] === "za"');
    assert.sameValue(stringKeys[12], 'zz', 'stringKeys[12] === "zz"');
    assert.sameValue(stringKeys[13], '\u03bb', 'stringKeys[13] === "\u03bb"');
    assert.sameValue(stringKeys[14], '\u03bc', 'stringKeys[14] === "\u03bc"');
    assert.sameValue(stringKeys[15], '\u03c0', 'stringKeys[15] === "\u03c0"');

    var allKeys = Reflect.ownKeys(ns);
    assert(
      allKeys.length >= 17,
      'at least as many keys as defined by the module and the specification'
    );
    assert.sameValue(allKeys[0], '$', 'allKeys[0] === "$"');
    assert.sameValue(allKeys[1], '$$', 'allKeys[1] === "$$"');
    assert.sameValue(allKeys[2], 'A', 'allKeys[2] === "A"');
    assert.sameValue(allKeys[3], 'Z', 'allKeys[3] === "Z"');
    assert.sameValue(allKeys[4], '_', 'allKeys[4] === "_"');
    assert.sameValue(allKeys[5], '__', 'allKeys[5] === "__"');
    assert.sameValue(allKeys[6], 'a', 'allKeys[6] === "a"');
    assert.sameValue(allKeys[7], 'aa', 'allKeys[7] === "aa"');
    assert.sameValue(allKeys[8], 'az', 'allKeys[8] === "az"');
    assert.sameValue(allKeys[9], 'default', 'allKeys[9] === "default"');
    assert.sameValue(allKeys[10], 'z', 'allKeys[10] === "z"');
    assert.sameValue(allKeys[11], 'za', 'allKeys[11] === "za"');
    assert.sameValue(allKeys[12], 'zz', 'allKeys[12] === "zz"');
    assert.sameValue(allKeys[13], '\u03bb', 'allKeys[13] === "\u03bb"');
    assert.sameValue(allKeys[14], '\u03bc', 'allKeys[14] === "\u03bc"');
    assert.sameValue(allKeys[15], '\u03c0', 'allKeys[15] === "\u03c0"');
    assert(
      allKeys.indexOf(Symbol.toStringTag) > 15,
      'keys array includes Symbol.toStringTag'
    );

}).then($DONE, $DONE).catch($DONE);

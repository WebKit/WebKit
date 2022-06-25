// This file was procedurally generated from the following sources:
// - src/dynamic-import/ns-delete-exported-init-no-strict.case
// - src/dynamic-import/namespace/promise.template
/*---
description: The [[Delete]] behavior for a key that describes an initialized exported binding on non strict mode (value from promise then)
esid: sec-finishdynamicimport
features: [dynamic-import]
flags: [generated, noStrict, async]
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


    [...]
    2. If Type(P) is Symbol, then
      a. Return ? OrdinaryDelete(O, P).
    3. Let exports be O.[[Exports]].
    4. If P is an element of exports, return false.
    5. Return true.

---*/

import('./module-code_FIXTURE.js').then(ns => {

    assert.sameValue(delete ns.default, false, 'delete: default');
    assert.sameValue(
      Reflect.deleteProperty(ns, 'default'), false, 'Reflect.deleteProperty: default'
    );
    assert.sameValue(ns.default, 42, 'binding unmodified: default');

    assert.sameValue(delete ns.local1, false, 'delete: local1');
    assert.sameValue(
      Reflect.deleteProperty(ns, 'local1'), false, 'Reflect.deleteProperty: local1'
    );
    assert.sameValue(ns.local1, 'Test262', 'binding unmodified: local1');

    assert.sameValue(delete ns.renamed, false, 'delete: renamed');
    assert.sameValue(
      Reflect.deleteProperty(ns, 'renamed'), false, 'Reflect.deleteProperty: renamed'
    );
    assert.sameValue(ns.renamed, 'TC39', 'binding unmodified: renamed');

    assert.sameValue(delete ns.indirect, false, 'delete: indirect');
    assert.sameValue(
      Reflect.deleteProperty(ns, 'indirect'),
      false,
      'Reflect.deleteProperty: indirect'
    );
    assert.sameValue(ns.indirect, 'Test262', 'binding unmodified: indirect');

}).then($DONE, $DONE).catch($DONE);

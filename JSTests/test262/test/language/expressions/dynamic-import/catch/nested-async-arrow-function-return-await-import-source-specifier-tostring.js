// This file was procedurally generated from the following sources:
// - src/dynamic-import/import-source-specifier-tostring.case
// - src/dynamic-import/catch/nested-async-arrow-fn-return-await.template
/*---
description: ToString value of specifier (nested in async arrow function, returned)
esid: sec-import-call-runtime-semantics-evaluation
features: [source-phase-imports, dynamic-import]
flags: [generated, async]
info: |
    ImportCall :
        import( AssignmentExpression )

    1. Let referencingScriptOrModule be ! GetActiveScriptOrModule().
    2. Assert: referencingScriptOrModule is a Script Record or Module Record (i.e. is not null).
    3. Let argRef be the result of evaluating AssignmentExpression.
    4. Let specifier be ? GetValue(argRef).
    5. Let promiseCapability be ! NewPromiseCapability(%Promise%).
    6. Let specifierString be ToString(specifier).
    7. IfAbruptRejectPromise(specifierString, promiseCapability).
    8. Perform ! HostImportModuleDynamically(referencingScriptOrModule, specifierString, promiseCapability).
    9. Return promiseCapability.[[Promise]].


    Import Calls

    Runtime Semantics: Evaluation

    ImportCall : import . source ( AssignmentExpression )
    1. Return ? EvaluateImportCall(AssignmentExpression, source).

    13.3.10.1.1 EvaluateImportCall ( specifierExpression, phase )
    1. Let referrer be GetActiveScriptOrModule().
    2. If referrer is null, set referrer to the current Realm Record.
    3. Let specifierRef be ? Evaluation of specifierExpression.
    4. Let specifier be ? GetValue(specifierRef).
    5. Let promiseCapability be ! NewPromiseCapability(%Promise%).
    6. Let specifierString be Completion(ToString(specifier)).
    7. IfAbruptRejectPromise(specifierString, promiseCapability).
    8. Let moduleRequest be a new ModuleRequest Record { [[Specifier]]: specifierString, [[Phase]]: phase }.
    9. Perform HostLoadImportedModule(referrer, moduleRequest, empty, promiseCapability).
    10. Return promiseCapability.[[Promise]].

    16.2.1.7.2 GetModuleSource ( )
    Source Text Module Record provides a GetModuleSource implementation that always returns an abrupt completion indicating that a source phase import is not available.
    1. Throw a ReferenceError exception.

---*/
// The following case is equivalent of the call of:
// import.source('./empty_FIXTURE.js')

const obj = {
    toString() {
        return './empty_FIXTURE.js';
    }
};


const f = async () => await import.source(obj);

f().catch(error => {

  assert.sameValue(error.name, 'ReferenceError');

}).then($DONE, $DONE);

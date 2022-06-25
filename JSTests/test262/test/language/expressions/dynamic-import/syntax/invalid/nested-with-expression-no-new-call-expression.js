// This file was procedurally generated from the following sources:
// - src/dynamic-import/no-new-call-expression.case
// - src/dynamic-import/syntax/invalid/nested-with-expression.template
/*---
description: ImportCall is a CallExpression, it can't be preceded by the new keyword (nested with syntax in the expression position)
esid: sec-import-call-runtime-semantics-evaluation
features: [dynamic-import]
flags: [generated, noStrict]
negative:
  phase: parse
  type: SyntaxError
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


    CallExpression:
      ImportCall

    ImportCall :
        import( AssignmentExpression[+In, ?Yield] )
---*/

$DONOTEVALUATE();

with (new import('')) {}

// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Default "anonymous" class declaration containing a static `name` method is
    correctly initialized upon evaluation
esid: sec-moduleevaluation
info: |
    [...]
    16. Let result be the result of evaluating module.[[ECMAScriptCode]].
    [...]

    15.2.3.11 Runtime Semantics: Evaluation

    ExportDeclaration : export default ClassDeclaration

    [...]
    3. Let className be the sole element of BoundNames of ClassDeclaration.
    4. If className is "*default*", then
       a. Let hasNameProperty be ? HasOwnProperty(value, "name").
       b. If hasNameProperty is false, perform SetFunctionName(value,
          "default").
       c. Let env be the running execution context's LexicalEnvironment.
       d. Perform ? InitializeBoundName("*default*", value, env).
    5. Return NormalCompletion(empty).
flags: [module]
---*/

export default class { static name() { return 'name method'; } }
import C from './eval-export-dflt-cls-name-meth.js';

assert.sameValue(
  C.name(), 'name method', '`name` property is not over-written'
);

// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Imported binding reflects state of exported `const` binding
esid: sec-moduledeclarationinstantiation
info: |
    [...]
    12. For each ImportEntry Record in in module.[[ImportEntries]], do
        a. Let importedModule be ? HostResolveImportedModule(module,
           in.[[ModuleRequest]]).
        b. If in.[[ImportName]] is "*", then
           [...]
        c. Else,
           i. Let resolution be ?
              importedModule.ResolveExport(in.[[ImportName]], « », « »).
           ii. If resolution is null or resolution is "ambiguous", throw a
               SyntaxError exception.
           iii. Call envRec.CreateImportBinding(in.[[LocalName]],
                resolution.[[Module]], resolution.[[BindingName]]).
    [...]
    16. Let lexDeclarations be the LexicallyScopedDeclarations of code.
    17. For each element d in lexDeclarations do
        a. For each element dn of the BoundNames of d do
           i, If IsConstantDeclaration of d is true, then
              1. Perform ! envRec.CreateImmutableBinding(dn, true).
           ii. Else,
               1. Perform ! envRec.CreateMutableBinding(dn, false).
           iii. If d is a GeneratorDeclaration production or a
                FunctionDeclaration production, then
                [...]

    8.1.1.5.5 CreateImportBinding

    [...]
    5. Create an immutable indirect binding in envRec for N that references M
       and N2 as its target binding and record that the binding is initialized.
    6. Return NormalCompletion(empty).
flags: [module]
---*/

assert.throws(ReferenceError, function() {
  typeof y;
}, 'binding is created but not initialized');

import { x as y } from './instn-named-bndng-const.js';
export const x = 23;

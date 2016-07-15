// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Modules are visited no more than one time when resolving bindings through
    "star" exports.
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

    15.2.1.16.3 ResolveExport( exportName, resolveSet, exportStarSet )

    [...]
    7. If exportStarSet contains module, return null.
    8. Append module to exportStarSet.
    [...]
negative: SyntaxError
flags: [module]
---*/

import { x } from './instn-named-star-cycle-2_FIXTURE.js';

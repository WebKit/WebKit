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
           i. Let namespace be ? GetModuleNamespace(importedModule).
           ii. Perform ! envRec.CreateImmutableBinding(in.[[LocalName]], true).
           iii. Call envRec.InitializeBinding(in.[[LocalName]], namespace).
        [...]

    15.2.1.16.3 ResolveExport( exportName, resolveSet, exportStarSet )

    [...]
    7. If exportStarSet contains module, return null.
    8. Append module to exportStarSet.
    [...]
negative: SyntaxError
flags: [module]
---*/

import * as ns from './instn-star-star-cycle-2_FIXTURE.js';

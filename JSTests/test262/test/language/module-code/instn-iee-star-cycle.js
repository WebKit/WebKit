// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Modules are visited no more than one time when resolving bindings through
    "star" exports.
esid: sec-moduledeclarationinstantiation
info: |
    [...]
    9. For each ExportEntry Record e in module.[[IndirectExportEntries]], do
       a. Let resolution be ? module.ResolveExport(e.[[ExportName]], « », « »).
       b. If resolution is null or resolution is "ambiguous", throw a
          SyntaxError exception.

    15.2.1.16.3 ResolveExport( exportName, resolveSet, exportStarSet )

    [...]
    7. If exportStarSet contains module, return null.
    8. Append module to exportStarSet.
    [...]
negative: SyntaxError
flags: [module]
---*/

export { x } from './instn-iee-star-cycle-2_FIXTURE.js';

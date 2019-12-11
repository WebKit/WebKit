// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: Module is instantiated exactly once
esid: sec-moduledeclarationinstantiation
info: |
  Instantiate( ) Concrete Method
    [...]
    4. Let result be InnerModuleInstantiation(module, stack, 0).
    [...]

  InnerModuleInstantiation( module, stack, index )
    [...]
    2. If module.[[Status]] is "instantiating", "instantiated", or "evaluated", then
      a. Return index.
    3. Assert: module.[[Status]] is "uninstantiated".
    4. Set module.[[Status]] to "instantiating".
    [...]
    9. For each String required that is an element of module.[[RequestedModules]], do
      a. Let requiredModule be ? HostResolveImportedModule(module, required).
      b. Set index to ? InnerModuleInstantiation(requiredModule, stack, index).
    [...]
flags: [module]
features: [export-star-as-namespace-from-module]
---*/

import {} from './instn-once.js';
import './instn-once.js';
import * as ns1 from './instn-once.js';
import dflt1 from './instn-once.js';
export {} from './instn-once.js';
import dflt2, {} from './instn-once.js';
export * from './instn-once.js';
export * as ns2 from './instn-once.js';
import dflt3, * as ns from './instn-once.js';
export default null;

let x;

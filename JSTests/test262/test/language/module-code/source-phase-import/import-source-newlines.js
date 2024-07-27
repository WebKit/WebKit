// Copyright (C) 2024 Chengzhong Wu. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
  import source in ImportDeclaration may include line terminators
esid: sec-modules
info: |
  ImportDeclaration:
    import source ImportedBinding FromClause ;

  This test uses all four LineFeed characters in order to completely verify the
  grammar.

  16.2.1.7.2 GetModuleSource ( )
  Source Text Module Record provides a GetModuleSource implementation that always returns an abrupt completion indicating that a source phase import is not available.
negative:
  phase: resolution
  type: SyntaxError
features: [source-phase-imports]
flags: [module]
---*/

$DONOTEVALUATE();

import "../resources/ensure-linking-error_FIXTURE.js";

import

  source

  y from '<do not resolve>';

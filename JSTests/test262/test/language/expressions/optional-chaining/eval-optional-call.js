// Copyright 2020 Toru Nagashima.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-optional-chaining-chain-evaluation
description: optional call invoked on eval function should be indirect eval.
info: |
  12.3.9.2 Runtime Semantics: ChainEvaluation
    OptionalChain: ?. Arguments
      1. Let thisChain be this OptionalChain.
      2. Let tailCall be IsInTailPosition(thisChain).
      3. Return ? EvaluateCall(baseValue, baseReference, Arguments, tailCall).

features: [optional-chaining]
---*/

const a = "global";
const b = (a => eval?.("a"))("local")

assert.sameValue(b, a);

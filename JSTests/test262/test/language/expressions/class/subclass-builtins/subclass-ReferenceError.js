// This file was procedurally generated from the following sources:
// - src/subclass-builtins/ReferenceError.case
// - src/subclass-builtins/default/expression.template
/*---
description: new SubReferenceError() instanceof ReferenceError (Subclass instanceof Heritage)
flags: [generated]
---*/


const Subclass = class extends ReferenceError {}

const sub = new Subclass();
assert(sub instanceof Subclass);
assert(sub instanceof ReferenceError);

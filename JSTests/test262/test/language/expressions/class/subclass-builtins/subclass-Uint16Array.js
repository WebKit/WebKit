// This file was procedurally generated from the following sources:
// - src/subclass-builtins/Uint16Array.case
// - src/subclass-builtins/default/expression.template
/*---
description: new SubUint16Array() instanceof Uint16Array (Subclass instanceof Heritage)
features: [TypedArray, Uint16Array]
flags: [generated]
---*/


const Subclass = class extends Uint16Array {}

const sub = new Subclass();
assert(sub instanceof Subclass);
assert(sub instanceof Uint16Array);

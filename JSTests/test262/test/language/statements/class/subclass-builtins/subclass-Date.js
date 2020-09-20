// This file was procedurally generated from the following sources:
// - src/subclass-builtins/Date.case
// - src/subclass-builtins/default/statement.template
/*---
description: new SubDate() instanceof Date (Subclass instanceof Heritage)
flags: [generated]
---*/


class Subclass extends Date {}

const sub = new Subclass();
assert(sub instanceof Subclass);
assert(sub instanceof Date);

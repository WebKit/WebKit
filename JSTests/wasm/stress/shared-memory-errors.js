//Growing a shared memory requires signal handlers, which are not yet ported to ARMv7
//@ skip if $architecture == "arm"
import * as assert from '../assert.js';

assert.throws(() => {
    new WebAssembly.Memory({ initial: 1, shared: true });
}, TypeError, `'maximum' page count must be defined if 'shared' is true`);

assert.throws(() => {
    let memory = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });
    memory.grow(3);
}, RangeError, `WebAssembly.Memory.grow would exceed the memory's declared maximum size`);

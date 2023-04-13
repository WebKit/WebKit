//Growing a shared memory requires signal handlers, which are not yet ported to ARMv7
if (typeof WebAssembly !== 'undefined')
    (new WebAssembly.Memory({initial: 0, maximum: 1, shared: true})).grow(1)

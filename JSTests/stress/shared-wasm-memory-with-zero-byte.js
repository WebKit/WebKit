if (typeof WebAssembly !== 'undefined')
    (new WebAssembly.Memory({initial: 0, maximum: 1, shared: true})).grow(1)

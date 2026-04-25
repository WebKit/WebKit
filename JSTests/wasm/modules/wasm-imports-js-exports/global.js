export const count = new WebAssembly.Global({
    value: 'i32',
    mutable: true,
}, 42);

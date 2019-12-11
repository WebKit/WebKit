var originalModule = globalThis.Module = {};
WebAssembly.Module;
if (Module !== originalModule)
    throw new Error('Global property `Module` was overwritten!');

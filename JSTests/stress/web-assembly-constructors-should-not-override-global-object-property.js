var originalModule = this.Module = {};
WebAssembly.Module;
if (Module !== originalModule)
    throw new Error('Global property `Module` was overwritten!');

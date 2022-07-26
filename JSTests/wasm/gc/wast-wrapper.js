// Load the compiled-to-JS reference interpreter for GC proposal.
load("wast.js");

export function compile(wat) {
    const binary = WebAssemblyText.encode(wat);
    return new WebAssembly.Module(binary);
}

export function instantiate(wat, imports = {}) {
    const module = compile(wat);
    return new WebAssembly.Instance(module, imports);
}

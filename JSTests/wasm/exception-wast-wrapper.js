globalThis.console = { log: print }

load("./exception-spec-wast.js", "caller relative");

export function encode(wat) {
    return WebAssemblyText.encode(wat);
}

export function decode(binary) {
    return WebAssemblyText.decode(binary);
}

export async function compile(wat) {
    return WebAssembly.compile(encode(wat));
}

export async function instantiate(wat, imports = {}) {
    let result = await WebAssembly.instantiate(encode(wat), imports);
    return result.instance;
}


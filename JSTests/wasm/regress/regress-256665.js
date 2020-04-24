//@ requireOptions("--useConcurrentJIT=false", "--jitPolicyScale=0")

function f() {
    var buffer = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 6, 1, 96, 1, 127, 1, 127, 3, 2, 1, 0, 5, 3, 1, 0, 0, 7, 8, 1, 4, 108, 111, 97, 100, 0, 0, 10, 9, 1, 7, 0, 32, 0, 40, 0, 100, 11]);
    var module = new WebAssembly.Module(buffer);
    var instance = new WebAssembly.Instance(module);
    try { instance.exports.load(0x10000 - 100 - 4); } catch (e) {}
    (555)[0];
}

f();
f();

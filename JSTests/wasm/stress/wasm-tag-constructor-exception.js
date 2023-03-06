const v0 = [];
const v2 = WebAssembly.Tag;
function f3(a4) {
    return a4;
}
Object.defineProperty(v2, "get", { enumerable: true, value: f3 });
const o5 = {
    "parameters": v0,
};
const v7 = new Proxy(v2, v2);
try {
    new v7(o5);
} catch { }

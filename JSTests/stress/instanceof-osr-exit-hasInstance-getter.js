function F1() {
    Object instanceof Proxy;
}
noInline(F1);

let count = 0;
function f20() {
    count++;
    OSRExit();
    return () => { };
}
Object.defineProperty(Proxy, Symbol.hasInstance, { get: f20 });

globalThis.testLoopCount ??= 1e4;
for (let i = 0; i < testLoopCount; i++) {
    F1();
}
if (count != testLoopCount)
    throw new Error("bad!");

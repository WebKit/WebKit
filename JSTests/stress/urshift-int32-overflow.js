//@ requireOptions("--forceEagerCompilation=1")

function foo() {
    const v22 = [];
    for (let i = 0; i < 3; i++) {
        for (let j = 0; j < 8; j++) {
            ({x: -766834598 >>> !v22});
        }
        (function v31(v32) { })();
    }
    return {};
}

const v2 = [];
const proxy = new Proxy(Array, { getPrototypeOf: foo });
for (let i = 0; i < 1000; i++) {
    v2.__proto__ = proxy;
}

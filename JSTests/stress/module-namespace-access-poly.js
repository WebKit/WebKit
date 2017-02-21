function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function access(ns)
{
    return ns.test;
}
noInline(access);

import("./resources/module-namespace-access.js").then((ns) => {
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(access(ns), 42);
    }
    let nonNS = { test: 50 };
    let nonNS2 = { ok: 22, test: 52 };
    for (var i = 0; i < 1e4; ++i) {
        shouldBe(access(ns), 42);
        shouldBe(access(nonNS), 50);
        shouldBe(access(nonNS2), 52);
    }
});
drainMicrotasks();

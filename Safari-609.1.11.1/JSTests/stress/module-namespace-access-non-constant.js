function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

import("./resources/module-namespace-access.js").then((ns) => {
    ns.change();
    for (var i = 0; i < 1e6; ++i) {
        shouldBe(ns.test, 55);
        shouldBe(ns.cocoa(), 55);
    }
});
drainMicrotasks();

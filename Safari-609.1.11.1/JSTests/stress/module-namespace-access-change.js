function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function access(namespace)
{
    return namespace.test;
}
noInline(access);

import("./resources/module-namespace-access.js").then((ns) => {
    for (var i = 0; i < 1e4; ++i)
        shouldBe(access(ns), 42)
    ns.change();
    for (var i = 0; i < 1e4; ++i)
        shouldBe(access(ns), 55)
});
drainMicrotasks();

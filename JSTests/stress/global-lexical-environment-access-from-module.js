function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let test = 42;
import("./resources/global-lexical-environment-access-from-module-child.js").then(function (namespace) {
    shouldBe(namespace.read(), 42);
    namespace.write(50);
    shouldBe(namespace.read(), 50);
    shouldBe(test, 50);
}).catch($vm.abort);

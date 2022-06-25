import { shouldBe } from "./resources/assert.js";

var originalThen = Promise.prototype.then;
Promise.prototype.then = undefined;
originalThen.call(import("./breaking-builtin-promise-then-does-not-break-internal-promise/test.js"), function (namespace) {
    shouldBe(namespace.result, 42);
});
drainMicrotasks();

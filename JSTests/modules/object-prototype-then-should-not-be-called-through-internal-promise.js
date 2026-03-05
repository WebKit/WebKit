import { shouldBe } from "./resources/assert.js";

var thenCalled = false;
Object.prototype.then = function() {
    thenCalled = true;
    throw new Error("Object.prototype.then should not be called");
};

var originalThen = Promise.prototype.then;
originalThen.call(import("./object-prototype-then-should-not-be-called-through-internal-promise/test.js"), function (namespace) {
    shouldBe(namespace.result, 42);
    shouldBe(thenCalled, false);
});
drainMicrotasks();
shouldBe(thenCalled, false);

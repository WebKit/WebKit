function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var resolved = null;
var rejected = null;
(async function () {
    resolved = await Promise.resolve(42);
    try {
        await Promise.reject(40);
    } catch (error) {
        rejected = error;
    }
})();
drainMicrotasks();
shouldBe(resolved, 42);
shouldBe(rejected, 40);

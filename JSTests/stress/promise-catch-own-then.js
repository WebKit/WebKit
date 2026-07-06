function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function target(promise) {
    return promise.catch(() => {});
}
noInline(target);

for (let i = 0; i < testLoopCount; i++)
    target(Promise.resolve(i));

let called = 0;
const promise = Promise.resolve(42);
promise.then = function(onFulfilled, onRejected) {
    called++;
};
target(promise);
shouldBe(called, 1);

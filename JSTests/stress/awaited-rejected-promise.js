//@ requireOptions("--useAsyncStackTrace=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

async function foo() {
   await Promise.reject(40);
};

let called = false;
foo().catch(e => {
  called = true;
  shouldBe(e, 40);
})
drainMicrotasks();

shouldBe(called, true);

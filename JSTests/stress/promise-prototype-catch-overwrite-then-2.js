function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

const promise = Promise.resolve(30);

function test() {
  let called = 0;
  promise.then = () => {
    called++;
  };
  for (let i = 0; i < testLoopCount; i++) {
    promise.catch(() => {});
  }
  shouldBe(called, testLoopCount);
}
test();

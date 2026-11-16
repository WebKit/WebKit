function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

const result = Promise.race();

shouldBe(result instanceof Promise, true);
let error;
result.catch(e => {
  error = e;
})
drainMicrotasks();
shouldBe(error instanceof TypeError, true);

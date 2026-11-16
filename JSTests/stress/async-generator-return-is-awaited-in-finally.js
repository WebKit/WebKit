function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

let finallyBlockCount = 0;
async function* foo(promise) {
  try { return promise; }
  finally { finallyBlockCount++; }
}

let iteratorResult;
foo(Promise.resolve(42)).next().then(arg => { iteratorResult = arg; });
drainMicrotasks();
shouldBe(finallyBlockCount, 1);
shouldBe(iteratorResult.value, 42);

let rejectReason;
foo(Promise.reject("err")).next().catch(arg => { rejectReason = arg; });
drainMicrotasks();
shouldBe(finallyBlockCount, 2);
shouldBe(rejectReason, "err");

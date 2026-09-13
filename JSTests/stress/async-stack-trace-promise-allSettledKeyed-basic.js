//@ requireOptions("--usePromiseAllKeyed=1")

const source = "async-stack-trace-promise-allSettledKeyed-basic.js";

function nop() {}

function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${a} to be ${b}`);
}

function testStack(error, stackFunctions) {
  const stackTrace = error.stack;
  if (!stackTrace) {
    throw new Error("Expected error to have stack trace, but it was undefined");
  }
  const stackLines = stackTrace.split('\n').filter(line => line.trim());
  for (let i = 0; i < stackFunctions.length; i++) {
    const [expectedFunction, expectedLocation] = stackFunctions[i];
    const isNativeCode = expectedLocation === "[native code]"
    const stackLine = stackLines[i];

    let found = false;

    if (isNativeCode) {
      if (stackLine === `${expectedFunction}@[native code]`)
        found = true;
    } else {
      if (stackLine === `${expectedFunction}@${source}:${expectedLocation}`)
        found = true;
      if (stackLine === `${expectedFunction}@${source}`)
        found = true;
    }

    if (!found) {
      throw new Error(
        `Expected stack trace to contain '${expectedFunction}' at '${expectedLocation}', but got '${stackLine}'` +
        `\nActual stack trace:\n${stackTrace}\n`
      );
    }
  }
}

function unwrap(promise) {
  let result;
  promise.then(value => result = value);
  drainMicrotasks();
  return result;
}

{
  async function fine() { }
  async function thrower() { await fine(); throw new Error('error'); }
  async function run() {
    return await Promise.allSettledKeyed({ a: thrower() });
  }

  for (let i = 0; i < testLoopCount; i++) {
    const results = unwrap(run());
    shouldBe(results.a.status, "rejected");
    testStack(results.a.reason, [
      ["thrower", "53:59"],
      ["async run", "55:41"],
      ["drainMicrotasks", "[native code]"],
      ["unwrap", "47:18"],
      ["global code", "59:27"]
    ]);
  }
}

{
  async function fine() { }
  async function task1() {

    await nop();

    throw new Error("error from task1");
  }
  async function task2() {
    await nop();



    throw new Error('error from task2');
  }
  async function task3() { await 1; throw new Error("error from task3"); }
  async function run() {
    return await Promise.allSettledKeyed({ f1: fine(), t1: task1(), f2: fine(), t2: task2(), t3: task3() });
  }

  for (let i = 0; i < testLoopCount; i++) {
    const results = unwrap(run());
    shouldBe(results.f1.status, "fulfilled");
    shouldBe(results.t1.status, "rejected");
    shouldBe(results.f2.status, "fulfilled");
    shouldBe(results.t2.status, "rejected");
    shouldBe(results.t3.status, "rejected");
    testStack(results.t1.reason, [
      ["task1", "77:20"],
      ["async run", "88:41"],
      ["drainMicrotasks", "[native code]"],
      ["unwrap", "47:18"],
      ["global code", "92:27"]
    ]);
    testStack(results.t2.reason, [
      ["task2", "84:20"],
      ["async run", "88:41"],
      ["drainMicrotasks", "[native code]"],
      ["unwrap", "47:18"],
      ["global code", "92:27"]
    ]);
    testStack(results.t3.reason, [
      ["task3", "86:52"],
      ["async run", "88:41"],
      ["drainMicrotasks", "[native code]"],
      ["unwrap", "47:18"],
      ["global code", "92:27"]
    ]);
  }
}

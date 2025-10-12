//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-promise-any-basic.js";

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

function shouldThrowAsync(run, errorType) {
  let actual;
  var hadError = false;
  run().then(
    function (value) {
      actual = value;
    },
    function (error) {
      hadError = true;
      actual = error;
    },
  );
  drainMicrotasks();
  if (!hadError) {
    throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
  }
  if (!(actual instanceof errorType)) {
    throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
  }
  return actual;
}

{
  async function fine() { }
  async function thrower() { await fine(); throw new Error('error'); }
  async function run() { await Promise.any([thrower()]); }

  for (let i = 0; i < testLoopCount; i++) {
    const aggregateError = shouldThrowAsync(
      async function test() {
        await run();
      }, AggregateError
    );
    drainMicrotasks();
    shouldBe(aggregateError.errors.length, 1);
    testStack(aggregateError.errors[0],
      [
        ["thrower", "68:59"],
        ["async run", "69:43"],
        ["async test", "74:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "56:18"],
        ["global code", "72:44"]
      ]
    );
  }
}

{
  async function task1() {
    
    await nop();
  
    throw new Error("error from task1");
  }
  async function task2() {
    await nop();



    throw new Error('task2 error');
  }
  async function task3() { await 1; throw new Error("error from task3"); }
  async function run() { await Promise.any([task1(), task2(), task3()]); }

  for (let i = 0; i < testLoopCount; i++) {
    const aggregateError = shouldThrowAsync(
      async function test() {
        await run();
      }, AggregateError
    );
    drainMicrotasks();
    shouldBe(aggregateError.errors.length, 3);
    testStack(aggregateError.errors[0],
      [
        ["task1", "97:20"],
        ["async run", "107:43"],
        ["async test", "112:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "56:18"],
        ["global code", "110:44"]
      ]
    );
    testStack(aggregateError.errors[1],
      [
        ["task2", "104:20"],
        ["async run", "107:43"],
        ["async test", "112:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "56:18"],
        ["global code", "110:44"]
      ]
    );
    testStack(aggregateError.errors[2],
      [
        ["task3", "106:52"],
        ["async run", "107:43"],
        ["async test", "112:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "56:18"],
        ["global code", "110:44"]
      ]
    );
  }
}

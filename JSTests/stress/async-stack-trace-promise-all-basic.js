//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-promise-all-basic.js";

function nop() {}

function shouldThrowAsync(run, errorType, message, stackFunctions) {
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
    if (message !== void 0 && actual.message !== message) {
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
    }

    const stackTrace = actual.stack;
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

{
  async function fine() { }
  async function thrower() { await fine(); throw new Error('error'); }
  async function run() { await Promise.all([fine(), thrower()]); }

  for (let i = 0; i < testLoopCount; i++) {
       shouldThrowAsync(
           async function test() {
               await run();
           }, Error, "error",
           [
               ["thrower", "65:59"],
               ["async run", "66:43"],
               ["async test", "71:25"],
               ["drainMicrotasks", "[native code]"],
               ["shouldThrowAsync", "19:20"],
               ["global code", "69:24"]
           ],
       );
       drainMicrotasks();
   }
}

{
  async function task1() { await nop(); }
  async function task2() {
    await nop();



    throw new Error('task2 error');
  }
  async function task3() { await nop(); }
  async function runner() { await Promise.all([task1(), task2(), task3()]); }

  for (let i = 0; i < testLoopCount; i++) {
       shouldThrowAsync(
           async function test() {
               await runner();
           }, Error, "task2 error",
           [
               ["task2", "93:20"],
               ["async runner", "96:46"],
               ["async test", "101:28"],
               ["drainMicrotasks", "[native code]"],
               ["shouldThrowAsync", "19:20"],
               ["global code", "99:24"]
           ],
       );
       drainMicrotasks();
   }
}

{
  async function task1() {}

  async function task2() {
    await foo();
  }
  async function foo() {
    await bar();
  }
  async function bar() {
    await 1;
    await Promise.all([task1(), baz()]);
  }
  async function baz() {
    await 2;
    throw new Error('task2 error');
  }

  async function task3() {}
  async function runner() { await Promise.all([task1(), task2(), task3()]); }

  for (let i = 0; i < testLoopCount; i++) {
       shouldThrowAsync(
           async function test() {
               await runner();
           }, Error, "task2 error",
           [
               ["baz", "131:20"],
               ["async bar", "127:22"],
               ["async foo", "123:14"],
               ["async task2", "120:14"],
               ["async runner", "135:46"],
               ["async test", "140:28"],
               ["drainMicrotasks", "[native code]"],
               ["shouldThrowAsync", "19:20"],
               ["global code", "138:24"]
           ],
       );
       drainMicrotasks();
   }
}

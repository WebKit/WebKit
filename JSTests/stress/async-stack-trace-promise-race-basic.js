//@ requireOptions("--useAsyncStackTrace=1")

const source = "async-stack-trace-promise-race-basic.js";

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
  async function run() { await Promise.race([thrower()]); }

  for (let i = 0; i < testLoopCount; i++) {
    shouldThrowAsync(
      async function test() {
        await run();
      }, Error, 'error', [
        ["thrower", "65:59"],
        ["async run", "66:44"],
        ["async test", "71:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "19:20"],
        ["global code", "69:21"]
      ] 
    );
    drainMicrotasks();
  }
}

{
  async function task1() {
    
    await nop();
  
    throw new Error("error from task1");
  }
  async function task2() {
    await nop();



    throw new Error('error from task2');
  }
  async function task3() { await 1; throw new Error("error from task3"); }
  async function run() { await Promise.race([task1(), task2(), task3()]); }

  for (let i = 0; i < testLoopCount; i++) {
    shouldThrowAsync(
      async function test() {
        await run();
      }, Error, 'error from task1' , [
        ["task1", "90:20"],
        ["async run", "100:44"],
        ["async test", "105:18"],
        ["drainMicrotasks", "[native code]"],
        ["shouldThrowAsync", "19:20"],
        ["global code", "103:21"]
      ]
    );
    drainMicrotasks();
  }
}

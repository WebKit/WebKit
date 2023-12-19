const log = msg => { logsActual.push(msg); };
const logsActual = [];
const logsExpected = `
CAUGHT broken promise #2
CAUGHT broken promise #4
CAUGHT broken promise #6
CAUGHT broken promise #8
Promise.all() called
tick #1
REJECTED broken promise #1
tick #2
tick #3
REJECTED broken promise #3
tick #4
tick #5
REJECTED broken promise #5
tick #6
tick #7
REJECTED broken promise #7
tick #8
Promise.all() resolved
`.trim();

function shouldBeRejected(promise) {
  return new Promise((resolve, reject) => {
    promise.then(reject, err => {
      log(`REJECTED ${err.message}`);
      resolve();
    });
  });
}

function makeTick(id) {
  Promise.resolve().then(() => {
    log(`tick #${id}`);
  });
}

function makeBrokenPromise(id) {
  const brokenPromise = Promise.resolve(id);
  Object.defineProperty(brokenPromise, "constructor", {
    get() { throw new Error(`broken promise #${id}`); },
  });
  return brokenPromise;
}

async function brokenPromiseInAwaitOfAsyncFunction() {
  makeTick(1);
  await makeBrokenPromise(1);
}

async function brokenPromiseCaughtInAwaitOfAsyncFunction() {
  makeTick(2);
  try { await makeBrokenPromise(2); }
  catch (err) { log(`CAUGHT ${err.message}`); }
}

async function* brokenPromiseInAwaitOfAsyncGeneratorFunction() {
  makeTick(3);
  await makeBrokenPromise(3);
}

async function* brokenPromiseCaughtInAwaitOfAsyncGeneratorFunction() {
  makeTick(4);
  try { await makeBrokenPromise(4); }
  catch (err) { log(`CAUGHT ${err.message}`); }
}

async function* brokenPromiseInYieldOfAsyncGeneratorFunction() {
  makeTick(5);
  yield makeBrokenPromise(5);
}

async function* brokenPromiseCaughtInYieldOfAsyncGeneratorFunction() {
  makeTick(6);
  try { yield makeBrokenPromise(6); }
  catch (err) { log(`CAUGHT ${err.message}`); }
}

async function* brokenPromiseInReturnOfAsyncGeneratorFunction() {
  makeTick(7);
  return makeBrokenPromise(7);
}

async function* brokenPromiseCaughtInReturnOfAsyncGeneratorFunction() {
  makeTick(8);
  try { return makeBrokenPromise(8); }
  catch (err) { log(`CAUGHT ${err.message}`); }
}

Promise.all([
  shouldBeRejected(brokenPromiseInAwaitOfAsyncFunction()),
  brokenPromiseCaughtInAwaitOfAsyncFunction(),
  shouldBeRejected(brokenPromiseInAwaitOfAsyncGeneratorFunction().next()),
  brokenPromiseCaughtInAwaitOfAsyncGeneratorFunction().next(),
  shouldBeRejected(brokenPromiseInYieldOfAsyncGeneratorFunction().next()),
  brokenPromiseCaughtInYieldOfAsyncGeneratorFunction().next(),
  shouldBeRejected(brokenPromiseInReturnOfAsyncGeneratorFunction().next()),
  brokenPromiseCaughtInReturnOfAsyncGeneratorFunction().next(),
  log("Promise.all() called"),
]).then(() => {
  log("Promise.all() resolved");
});

drainMicrotasks();
if (logsActual.join("\n") !== logsExpected)
  throw new Error(`Bad logs!\n\n${logsActual.join("\n")}`)

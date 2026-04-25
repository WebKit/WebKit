function shouldThrowAsync(run, errorType) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
}

shouldThrowAsync(async function () {
  const nonThenable = {};
  Promise.resolve = function (v) { return v; };
  await Promise.race([nonThenable]);
}, TypeError);

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var executorFunction;
function NotPromise(executor) {
  executorFunction = executor;
  executor(function(){}, function(){});
}
Promise.resolve.call(NotPromise);

shouldBe(JSON.stringify(Object.getOwnPropertyNames(executorFunction).sort()), `["length","name"]`);
shouldBe(executorFunction.hasOwnProperty('name'), true);
shouldBe(executorFunction.name, ``);
shouldBe(delete executorFunction.name, true);

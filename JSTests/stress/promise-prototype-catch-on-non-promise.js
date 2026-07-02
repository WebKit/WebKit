//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

// PromisePrototypeCatchIntrinsic guards the receiver with Check(PromiseObjectUse),
// which fails with a BadType exit when the receiver is not a JSPromise. The
// recompile must not re-inline the intrinsic and loop, and must keep producing
// the spec result via the runtime path.

function callCatch(promise, onRejected)
{
    return promise.catch(onRejected);
}
noInline(callCatch);

var fake = Object.create(Promise.prototype);
fake.then = function(onFulfilled, onRejected) { return "ok"; };

for (var i = 0; i < 1e5; ++i) {
    var result = callCatch(fake, function() {});
    if (result !== "ok")
        throw new Error("bad result at " + i + ": " + result);
}

if (numberOfDFGCompiles(callCatch) > 2)
    throw new Error("too many recompiles: " + numberOfDFGCompiles(callCatch));

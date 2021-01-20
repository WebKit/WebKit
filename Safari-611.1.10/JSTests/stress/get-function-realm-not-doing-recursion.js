//@ skip if $buildType != "debug"
//@ runDefault("--useConcurrentJIT=0")

function canThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (errorThrown && String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
    return false;
}

const emptyFunction = function() {};

function makeLongProxyChain() {
  let p = new Proxy(emptyFunction, {});
  for (let i = 0; i < 200000; i++)
    p = new Proxy(p, {});
  return p;
}

let p = makeLongProxyChain();
canThrow(() => {
    Reflect.construct(Object, [], p);
}, `RangeError: Maximum call stack size exceeded.`);

//@ runDefault("--thresholdForJITAfterWarmUp=200", "--thresholdForOptimizeAfterWarmUp=200", "--thresholdForFTLOptimizeAfterWarmUp=5")
function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

let j = 0;

function foo(a0, a1, a2, a3, a4) {
  do {
    for (let k = 0; k < 1000; k++) {
      let x0 = [0];
    }
  } while (j++ < 2);
  isNaN(...a0);
}

for (let i=0; i<100; i++) {
  foo('', 0, 0, {});
}

shouldThrow(() => {
    [0, 0].reduce(foo);
}, `TypeError: Spread syntax requires ...iterable[Symbol.iterator] to be a function`);

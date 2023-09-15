//@ skip if $memoryLimited

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

shouldThrow(() => {
    let x = { toString: () => ''.padEnd(2 ** 31 - 1, 10..toLocaleString()) };
    let y = [x].join();
    // This is to force the resolution of the rope returned by x.toString() so that we'll
    // get the OOME even if the slowJoin path is taken.
    y[y.length - 1];
}, `RangeError: Out of memory`);

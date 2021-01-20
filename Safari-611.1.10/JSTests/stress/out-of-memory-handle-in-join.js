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
    [x].join();
}, `RangeError: Out of memory`);

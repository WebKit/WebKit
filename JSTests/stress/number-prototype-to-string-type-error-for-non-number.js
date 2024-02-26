let successCount = 0, errorCount = 0;

function numberToString(thisValue) {
    try {
        Number.prototype.toString.call(thisValue);
        successCount++;
    } catch {
        errorCount++;
    }
}

(function() {
    const runs = 1e4;

    for (let i = 0; i < runs; i++) {
        numberToString(null);
        numberToString(undefined);
        numberToString(true);
        numberToString(i);
    }

    if (successCount !== runs)
        throw new Error(`Bad success value: ${successCount}!`);

    if (errorCount !== runs * 3)
        throw new Error(`Bad errors value: ${errorCount}!`);
})();

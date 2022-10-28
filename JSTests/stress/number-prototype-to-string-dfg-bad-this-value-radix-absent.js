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
    const runs = 1e6;

    for (let i = 0; i < runs; i++) {
        numberToString({});
        numberToString(i);
    }

    if (successCount !== runs)
        throw new Error(`Bad value: ${successCount}!`);

    if (errorCount !== runs)
        throw new Error(`Bad value: ${errorCount}!`);
})();

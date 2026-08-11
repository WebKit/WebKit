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
    for (let i = 0; i < testLoopCount; i++) {
        numberToString({});
        numberToString(i);
    }

    if (successCount !== testLoopCount)
        throw new Error(`Bad value: ${successCount}!`);

    if (errorCount !== testLoopCount)
        throw new Error(`Bad value: ${errorCount}!`);
})();

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

if (Intl.DisplayNames) {
    shouldThrow(() => {
        new Intl.DisplayNames('en', "Hello");
    }, `TypeError: options argument is not an object or undefined`);
}

if (Intl.ListFormat) {
    shouldThrow(() => {
        new Intl.ListFormat('en', "Hello");
    }, `TypeError: options argument is not an object or undefined`);
}

if (Intl.Segmenter) {
    shouldThrow(() => {
        new Intl.Segmenter('en', "Hello");
    }, `TypeError: options argument is not an object or undefined`);
}

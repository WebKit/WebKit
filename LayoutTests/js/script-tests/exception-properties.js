description("Test for correct properties on Error objects.");

try {
    // generate a RangeError.
    [].length = -1;
} catch (rangeError) {
    var nativeError = rangeError;
    var error = new Error("message");

    shouldBe('Object.keys(error).sort()', '[]');
    shouldBe('Object.keys(nativeError).sort()', '[]');

    shouldBe('Object.getOwnPropertyNames(error).sort()', '["column", "line", "message", "sourceURL", "stack"]');
    shouldBe('Object.getOwnPropertyNames(nativeError).sort()', '["column", "line", "message", "sourceURL", "stack"]');

    shouldBe('Object.getPrototypeOf(nativeError).name', '"RangeError"');
    shouldBe('Object.getPrototypeOf(nativeError).message', '""');
}

description("Test for correct properties on Error objects.");

function enumerableProperties(object)
{
    var result = [];
    for (var i in object)
        result.push(i);
    return result;
}

try {
    // generate a RangeError.
    [].length = -1;
} catch (rangeError) {
    var nativeError = rangeError;
    var error = new Error("message");

    shouldBe('enumerableProperties(error)', '[]');
    shouldBe('enumerableProperties(nativeError)', '["line", "sourceURL", "stack"]');

    shouldBe('Object.getPrototypeOf(nativeError).name', '"RangeError"');
    shouldBe('Object.getPrototypeOf(nativeError).message', '""');
}

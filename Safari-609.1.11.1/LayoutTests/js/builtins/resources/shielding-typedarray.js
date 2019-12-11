description("Test to ensure TypedArray filter is shielded from Array.prototype.push modifications");

const ArrayPrototypePushBackup = Array.prototype.push;
Array.prototype.push = function()
{
    testFailed("Int8Array.prototype.push should not be called by builtin code");
    return ArrayPrototypePushBackup.apply(this, arguments);
}

try {
    array = Int8Array.from("1234").filter(function(value) {
        return value === 1;
    });
    shouldBeTrue("array.length === 1");
}
finally {
    Array.prototype.push = ArrayPrototypePushBackup;
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var names = [
    'global',
    'ignoreCase',
    'multiline',
    'source',
];

for (var name of names) {
    var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
    shouldBe(descriptor.get.name, 'get ' + name);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var strings = [
    "Cocoa",
    "Cappuccino",
    "Matcha",
    "Cocoa\u0080",
    "𠮷野家", // Japanese Beef-bowl shop name including surrogate-pairs.
    "📱",
];

for (let lhs of strings) {
    for (let rhs of strings) {
        let expected = $vm.make16BitStringIfPossible(lhs).localeCompare($vm.make16BitStringIfPossible(rhs));
        let result = lhs.localeCompare(rhs);
        shouldBe(result, expected);
    }
}

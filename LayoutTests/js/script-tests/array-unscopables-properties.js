description("Verify the various properties of Array.prototype[@@unscopables]");

shouldBeEqualToString("typeof Array.prototype[Symbol.unscopables]", "object");
shouldBe("Object.getPrototypeOf(Array.prototype[Symbol.unscopables])", "null");
shouldBeFalse("Object.getOwnPropertyDescriptor(Array.prototype, Symbol.unscopables).writable");
shouldBeFalse("Object.getOwnPropertyDescriptor(Array.prototype, Symbol.unscopables).enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype, Symbol.unscopables).configurable");

let expectedEntries = [
    "at",
    "copyWithin",
    "entries",
    "fill",
    "find",
    "findIndex",
    "findLast",
    "findLastIndex",
    "flat",
    "flatMap",
    "includes",
    "keys",
    "toReversed",
    "toSorted",
    "toSpliced",
    "values",
];
shouldBe("Object.getOwnPropertyNames(Array.prototype[Symbol.unscopables])", "expectedEntries");
shouldBe("Object.getOwnPropertySymbols(Array.prototype[Symbol.unscopables])", "[]");

for (let entry of expectedEntries) {
    shouldBeTrue("Array.prototype[Symbol.unscopables][\"" + entry + "\"]");
    shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype[Symbol.unscopables], \"" + entry + "\").writable");
    shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype[Symbol.unscopables], \"" + entry + "\").enumerable");
    shouldBeTrue("Object.getOwnPropertyDescriptor(Array.prototype[Symbol.unscopables], \"" + entry + "\").configurable");
}

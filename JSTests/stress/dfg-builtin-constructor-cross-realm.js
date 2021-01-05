function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}
noInline(shouldBe);

const {
    Array: OtherArray,
    String: OtherString,
    Object: OtherObject,
    Int8Array: OtherInt8Array,
} = createGlobalObject();

function newArray() { return new OtherArray(4); }
noInline(newArray);

function newString() { return new OtherString("foo"); }
noInline(newString);

function newObject() { return new OtherObject(); }
noInline(newObject);

function newInt8Array() { return new OtherInt8Array(4); }
noInline(newInt8Array);

(function() {
    for (let i = 0; i < 1e5; i++) {
        shouldBe(newArray().constructor, OtherArray);
        shouldBe(newString().constructor, OtherString);
        shouldBe(newObject().constructor, OtherObject);
        shouldBe(newInt8Array().constructor, OtherInt8Array);
    }
})();

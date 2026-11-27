function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + " expected: " + String(expected));
}

function read(array, index) {
    return array[index];
}

var other = createGlobalObject();
var otherArray = new other.Array(16);
otherArray[0] = {};
other.Array.prototype[3] = "other";

class SubArray extends Array { }
SubArray.prototype[3] = "subclass";
var subArray = new SubArray(16);
subArray[0] = {};

var reparentedArray = new Array(16);
reparentedArray[0] = {};
Object.setPrototypeOf(reparentedArray, { 3: "reparented" });

var array = new Array(16);
array[0] = {};
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(read(array, 5), undefined);
    shouldBe(read(array, 0), array[0]);
    shouldBe(read(otherArray, 3), "other");
    shouldBe(read(otherArray, 4), undefined);
    shouldBe(read(subArray, 3), "subclass");
    shouldBe(read(subArray, 4), undefined);
    shouldBe(read(reparentedArray, 3), "reparented");
    shouldBe(read(reparentedArray, 4), undefined);
}

Object.prototype[5] = "object";
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(read(array, 5), "object");
    shouldBe(read(array, 7), undefined);
    shouldBe(read(otherArray, 5), undefined);
}

Object.defineProperty(Array.prototype, 6, { get() { return "getter"; }, configurable: true });
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(read(array, 5), "object");
    shouldBe(read(array, 6), "getter");
    shouldBe(read(array, 7), undefined);
    shouldBe(read(otherArray, 5), undefined);
}

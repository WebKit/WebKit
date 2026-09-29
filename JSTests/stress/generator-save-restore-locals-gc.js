function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

function* holder(count) {
    let object = { value: 0 };
    let array = [];
    let string = "";
    for (let i = 1; i <= count; ++i) {
        object = { value: i };
        array = [i, i];
        string = String(i) + "!";
        yield i;
        shouldBe(object.value, i);
        shouldBe(array[0] + array[1], i * 2);
        shouldBe(string, String(i) + "!");
    }
    return object.value + array[0];
}

let count = 200;
let iterator = holder(count);
shouldBe(iterator.next().value, 1);
fullGC();
for (let i = 2; i <= count; ++i) {
    shouldBe(iterator.next().value, i);
    edenGC();
}
let result = iterator.next();
shouldBe(result.done, true);
shouldBe(result.value, count * 2);

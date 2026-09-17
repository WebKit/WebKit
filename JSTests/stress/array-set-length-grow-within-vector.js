function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function incrementLength(count, makeValue) {
    const array = [];
    for (let i = 0; i < count; i++) {
        ++array.length;
        array[i] = makeValue(i);
    }
    return array;
}

for (const makeValue of [i => i, i => i + 0.5, i => "v" + i]) {
    const array = incrementLength(150000, makeValue);
    shouldBe(array.length, 150000);
    for (const i of [0, 1, 99999, 100000, 100001, 149999])
        shouldBe(array[i], makeValue(i));
    shouldBe(array[150000], undefined);
}

{
    const array = incrementLength(150000, i => i);
    array.length = 120000;
    shouldBe(array.length, 120000);
    shouldBe(array[119999], 119999);
    shouldBe(array[120000], undefined);
    shouldBe(120000 in array, false);

    array.length = 140000;
    shouldBe(array.length, 140000);
    shouldBe(array[119999], 119999);
    for (const i of [120000, 130000, 139999]) {
        shouldBe(array[i], undefined);
        shouldBe(i in array, false);
    }
    array[139999] = "last";
    shouldBe(array[139999], "last");
    shouldBe(array.indexOf("last"), 139999);
}

{
    const array = [1, 2, 3];
    array.length = 110000;
    shouldBe(array.length, 110000);
    shouldBe(array[2], 3);
    shouldBe(3 in array, false);
    shouldBe(109999 in array, false);
    ++array.length;
    shouldBe(array.length, 110001);
    array[110000] = 4;
    shouldBe(array[110000], 4);
    shouldBe(Object.keys(array).length, 4);
    array.length = 2;
    shouldBe(array.length, 2);
    shouldBe(array[2], undefined);
    shouldBe(Object.keys(array).length, 2);
}

{
    const array = incrementLength(100000, i => i);
    array.length = 3000000;
    shouldBe(array.length, 3000000);
    shouldBe(array[99999], 99999);
    shouldBe(2999999 in array, false);
    array[2999999] = 1;
    shouldBe(array[2999999], 1);
    array.length = 100001;
    shouldBe(array.length, 100001);
    shouldBe(array[99999], 99999);
    shouldBe(array[100000], undefined);
}

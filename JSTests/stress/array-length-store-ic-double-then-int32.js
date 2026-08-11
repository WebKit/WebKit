function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("got " + actual + ", expected " + expected);
}

function f(a, n) { a.length = n; }
noInline(f);

let doubleArr = [1.5];
doubleArr.push(2.5);
let int32Arr = [1];
int32Arr.push(2);

for (let i = 0; i < testLoopCount; ++i) {
    doubleArr.push(3.5, 4.5, 5.5);
    int32Arr.push(3, 4, 5);
    f(doubleArr, 2);
    f(int32Arr, 2);
    shouldBe(doubleArr.length, 2);
    shouldBe(doubleArr[2], undefined);
    shouldBe(2 in doubleArr, false);
    shouldBe(int32Arr.length, 2);
    shouldBe(int32Arr[2], undefined);
    shouldBe(2 in int32Arr, false);
}

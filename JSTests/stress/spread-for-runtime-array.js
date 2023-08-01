function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var list = [
    [0, 1, 2, 3, 4, 5],
    [],
    ["Hello", "World"],
];

for (var i = 0; i < 1e4; ++i) {
    for (var array of list) {
        $vm.ensureArrayStorage(array);
        array.hey = 42;
        var iterator = array[Symbol.iterator]();
        while (!iterator.next().done);
    }
}

var runtimeArray = $vm.createRuntimeArray(0, 1, 2, 3, 4, 5, 6);
runtimeArray[10] = "Hello";
shouldBe(runtimeArray[10], "Hello");

function test(runtimeArray)
{
    var array = [...runtimeArray];
    shouldBe(array.length, 7);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(runtimeArray);

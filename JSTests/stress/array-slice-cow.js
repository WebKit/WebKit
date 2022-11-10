function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var allowDoubleShape = $vm.allowDoubleShape();

function testInt32()
{
    var array = [0, 1, 2, 3];
    var slice = array.slice(1);
    shouldBe($vm.indexingMode(array), "CopyOnWriteArrayWithInt32");
    shouldBe($vm.indexingMode(slice), "ArrayWithInt32");
    return slice;
}
noInline(testInt32);

function testDouble()
{
    var array = [0.1, 1.1, 2.1, 3.1];
    var slice = array.slice(1);
    if (allowDoubleShape) {
        shouldBe($vm.indexingMode(array), "CopyOnWriteArrayWithDouble");
        shouldBe($vm.indexingMode(slice), "ArrayWithDouble");
    } else {
        shouldBe($vm.indexingMode(array), "CopyOnWriteArrayWithContiguous");
        shouldBe($vm.indexingMode(slice), "ArrayWithContiguous");
    }
    return slice;
}
noInline(testDouble);

function testContiguous()
{
    var array = [true, false, true, false];
    var slice = array.slice(1);
    shouldBe($vm.indexingMode(array), "CopyOnWriteArrayWithContiguous");
    shouldBe($vm.indexingMode(slice), "ArrayWithContiguous");
    return slice;
}
noInline(testContiguous);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(testInt32()), `[1,2,3]`);
    shouldBe(JSON.stringify(testDouble()), `[1.1,2.1,3.1]`);
    shouldBe(JSON.stringify(testContiguous()), `[false,true,false]`);
}

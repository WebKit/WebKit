var custom = $vm.createCustomTestGetterSetter();
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var valueFunc = runString(`
var custom = $vm.createCustomTestGetterSetter();
function valueFunc() {
    return custom.customValueGlobalObject;
}`).valueFunc;

var accessorFunc = runString(`
var custom = $vm.createCustomTestGetterSetter();
function accessorFunc() {
    return custom.customAccessorGlobalObject;
}`).accessorFunc;

shouldBe(this.custom !== valueFunc().custom, true);
shouldBe(this.custom !== accessorFunc().custom, true);
shouldBe(this.custom, custom.customValueGlobalObject.custom);
shouldBe(this.custom, custom.customAccessorGlobalObject.custom);

function valueTest()
{
    return valueFunc();
}
noInline(valueTest);

function accessorTest()
{
    return accessorFunc();
}
noInline(accessorTest);

for (var i = 0; i < 1e3; ++i)
    shouldBe(this.custom !== valueTest().custom, true);
for (var i = 0; i < 1e3; ++i)
    shouldBe(this.custom !== accessorTest().custom, true);

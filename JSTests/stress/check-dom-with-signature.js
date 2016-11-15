function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];
for (var i = 0; i < 100; ++i)
    array.push(createDOMJITFunctionObject());

function calling(dom)
{
    return dom.func();
}
noInline(calling);

for (var i = 0; i < 1e3; ++i) {
    array.forEach((obj) => {
        shouldBe(calling(obj), 42);
    });
}

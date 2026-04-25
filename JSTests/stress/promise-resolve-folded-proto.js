function test(value)
{
    value.ok = 42;
    return Promise.resolve(value);
}
noInline(test);

var object = { };
for (var i = 0; i < testLoopCount; ++i) {
    test(object);
}

Object.prototype.then = function () {
    throw new Error("Hey");
};

test(object).then($vm.abort);

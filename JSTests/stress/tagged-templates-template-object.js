function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function tag(elements) {
    return function (siteObject) {
        shouldBe(siteObject instanceof Array, true);
        shouldBe(Object.isFrozen(siteObject), true);
        shouldBe(siteObject.raw instanceof Array, true);
        shouldBe(Object.isFrozen(siteObject.raw), true);
        shouldBe(siteObject.hasOwnProperty("raw"), true);
        shouldBe(siteObject.propertyIsEnumerable("raw"), false);
        shouldBe(siteObject.length, arguments.length);
        shouldBe(siteObject.raw.length, arguments.length);
        var count = siteObject.length;
        for (var i = 0; i < count; ++i) {
            shouldBe(siteObject.hasOwnProperty(i), true);
            var desc = Object.getOwnPropertyDescriptor(siteObject, i);
            shouldBe(desc.writable, false);
            shouldBe(desc.enumerable, true);
            shouldBe(desc.configurable, false);
        }
        shouldBe(siteObject.length, elements.length + 1);
        for (var i = 0; i < elements.length; ++i)
            shouldBe(arguments[i + 1], elements[i]);
    };
}

var value = {
    toString() {
        throw new Error('incorrect');
    },
    valueOf() {
        throw new Error('incorrect');
    }
};

tag([])``;
tag([])`Hello`;
tag([])`Hello World`;
tag([value])`Hello ${value} World`;
tag([value, value])`Hello ${value} OK, ${value}`;

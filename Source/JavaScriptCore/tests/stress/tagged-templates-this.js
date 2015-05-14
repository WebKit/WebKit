function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function tag() {
    "use strict";
    return this;
}

var object = {
    tag() {
        'use strict';
        return this;
    }
};

shouldBe(tag`Hello`, undefined);
shouldBe((function () { return tag }())`Hello`, undefined);
shouldBe(object.tag`Hello`, object);
shouldBe(object['tag']`Hello`, object);
shouldBe(object[(function () { return 'tag'; }())]`Hello`, object);

with (object) {
    shouldBe(tag`Hello`, object);
}

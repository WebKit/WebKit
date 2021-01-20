function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class A extends Function {}
shouldBe(new A("'use strict';").hasOwnProperty('arguments'), false);
shouldBe(new A().hasOwnProperty('arguments'), true);

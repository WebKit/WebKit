function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [0, 1, 2, 3];
shouldBe(`${array}`, `0,1,2,3`);

var array2 = [0, 1, 2, 3];
array2.join = function () { return `42`; }
shouldBe(`${array2}`, `42`);

class DerivedArray extends Array {
    constructor()
    {
        super(0, 1, 2, 3);
    }

    join()
    {
        return `0`;
    }
}
var derived = new DerivedArray();
shouldBe(`${derived}`, `0`);

var array3 = [0, 1, 2, 3];
array3.__proto__ = { toString: Array.prototype.toString, join() { return `41`; } };
shouldBe(`${array3}`, `41`);

Array.prototype.join = function() { return `40`; };
shouldBe(`${array}`, `40`);

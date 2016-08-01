
expected = Object.prototype.toString;
foo = {foo: 1, bar: 20};
delete foo.bar;


function test() {
    let toString = foo.toString;
    if (toString !== expected)
        throw new Error();
}

for (i = 0; i < 10; i++)
    test();

foo.toString = 100;
expected = 100;

test();

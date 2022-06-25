let warm = 1000;

function foo(f) {
    return f.arguments;
}
noInline(foo);

let caught = 0;

function bar() {
    for (let i = 0; i < warm; ++i)
        foo(bar);
    const x = function baz1() { "use strict"; return 42; };
    const y = function baz2() { "use strict"; return 0xc0defefe; };
    return [x, y];
}

bar();
bar();
const [baz1, baz2] = bar();


if (baz1() !== 42)
    throw new Error(`bad!`);

if (baz2() !== 0xc0defefe)
    throw new Error(`bad!`);

try {
    foo(baz1);
} catch (e) {
    ++caught;
}

try {
    foo(baz2);
} catch (e) {
    ++caught;
}

if (caught !== 2)
    throw new Error(`bad!`);

function assert(b, m = "Bad!") {
    if (!b) {
        throw new Error(m);
    }
}

function test(f, iters = 1000) {
    for (let i = 0; i < iters; i++)
        f(i);
}

function func(x) {
    return x;
}
noInline(func);

function check(index, arr, B)
{
    for (let i = 0; i < 1000; i++)
        assert(arr[i] instanceof B);
}
noInline(check);

test(function body(index) {
    class A {
        constructor(x, f = func)
        {
            this._value = x;
            this._func = f;
        }
    }

    class B extends A {
    }

    let arr = [];
    for (let i = 0; i < 1000; i++)
        arr.push(new B(20));

    check(index, arr, B);
}, 8);

function assert(b, m = "Bad!") {
    if (!b) {
        throw new Error(m);
    }
}

function test(f, iters = testLoopCount/10) {
    for (let i = 0; i < iters; i++)
        f(i);
}

function func(x) {
    return x;
}
noInline(func);

var n = 2;
var prototype = {};
function prep(index, i, A, B)
{
    if (index === (n - 1) && i === testLoopCount/2) {
        // Fire watchpoint!
        A.prototype = prototype;
    }
}

function check(index, arr, A, B, originalPrototype)
{
    if (index === (n - 1)) {
        assert(originalPrototype !== prototype);
        for (let i = 0; i < testLoopCount/2; i++)
            assert(arr[i].__proto__ === originalPrototype);
        for (let i = testLoopCount/2; i < testLoopCount; i++)
            assert(arr[i].__proto__ === prototype);
    } else {
        for (let i = 0; i < testLoopCount; i++)
            assert(arr[i].__proto__ === originalPrototype);
    }
}
noInline(check);

test(function body(index) {
    function A(x, f = func) {
        this._value = x;
        this._func = f;
    }

    function B(n)
    {
        return new A(n);
    }

    var originalPrototype = A.prototype;
    let arr = [];
    for (let i = 0; i < testLoopCount; i++) {
        prep(index, i, A, B);
        arr.push(B(20));
    }

    check(index, arr, A, B, originalPrototype);
}, n);

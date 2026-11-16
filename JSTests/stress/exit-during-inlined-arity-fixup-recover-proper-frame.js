var createBuiltin = $vm.createBuiltin;

let i;
function verify(a, b, c, d, e, f) {
    function assert(b, m) {
        if (!b)
            throw new Error(m);
    }
    assert(a === i);
    assert(b === i+1);
    assert(c === i+2);
    assert(d === null);
    assert(e === undefined);
    assert(f === undefined);
}
noInline(verify);

function func(a, b, c, d, e, f)
{
    verify(a, b, c, d, e, f);
    return !!(a%2) ? a + b + c + d : a + b + c + d;
}

const bar = createBuiltin(`(function (f, a, b, c, d) {
    let y = @idWithProfile(null, "SpecInt32Only");
    return f(a, b, c, y);
})`);

noInline(bar);

for (i = 0; i < 1000; ++i) {
    bar(func, i, i+1, i+2, i+3);
}

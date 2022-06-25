// Verify that we have the correct speculation checks for Math.pow(NaN, 0).

function func(x) {
    return fiatInt52(Math.pow(NaN, (x > 1)));
};

noInline(func);

function test(f)
{
    for (let i = 0; i < 10000; ++i) {
        if (f(0) != 1)
            throw "Wrong expected value";

        if (f(1) != 1)
            throw "Wrong expected value";
    }
}

test(func);


// This test verifies that we don't crash in FTL generated code due to lack of a store barrier
// for a put-by-val when we don't know when the value was allocated.

class MyNumber
{
    constructor(v)
    {
        this._v = v;
    }

    plusOne()
    {
        return this._v + 1;
    }
}

noDFG(MyNumber.plusOne);

let count = 0;
let bogus = null;

function bar()
{
    count++;

    if (!(count % 100))
        fullGC();
    return new MyNumber(count);
}

noDFG(bar);
noInline(bar);

function foo(index, arg)
{
    var result = [arg[0]];
    if (arg.length > 1)
        result[1] = bar();
    return result;
}

noInline(foo);

function test()
{
    for (let i = 0; i < 50000; i++)
    {
        let a = [1, i];
        let x = foo(i, a);

        if (!(count % 100))
            edenGC();

        for (let j = 0; j < 100; j++)
            bogus = new MyNumber(-1);
        
        if ((count + 1) != x[1].plusOne())
            throw("Wrong value for count");
    }
}

test();

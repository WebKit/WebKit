function test(value)
{
    return +value;
}

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i.toString());
if (result !== 9999)
    throw new Error(`bad result ${result}`);

function test(value)
{
    return Number(value);
}

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i.toString());

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i);

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i.toString());

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i);

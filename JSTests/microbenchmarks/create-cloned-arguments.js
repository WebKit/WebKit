function test(a, b, c)
{
    "use strict";
    return arguments;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(0, 1, 2);

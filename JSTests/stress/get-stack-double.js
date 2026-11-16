function bar(n)
{
    for (p = 0; p < 30; p++)
        if (p + 0.1)
            n -= 0.2
}

for (var i = 0; i < testLoopCount; ++i)
    bar(0);

function noInline() { }

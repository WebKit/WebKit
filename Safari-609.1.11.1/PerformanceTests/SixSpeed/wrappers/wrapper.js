function assertEqual(a, b)
{
}

var targetFunction;
function test(fn)
{
    targetFunction = fn;
    noInline(targetFunction);
}

function jscRun(iterations)
{
    var fn = targetFunction;
    for (var i = 0; i < iterations; i++)
        fn();

}

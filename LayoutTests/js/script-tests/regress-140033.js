description(
"Regression test for https://webkit.org/b/140033. This test should run without crashing."
);

function defineADeadFunction(x, y)
{
    var u;
    var a = u;
    var b = x;

    if (x > 1500)
        b -= y;

    var unused = function() {
        return 42;
    }

    return b;
}

var result = 0;
for (var i = 1; i < 2000; i++)
    result += defineADeadFunction(i, " ");

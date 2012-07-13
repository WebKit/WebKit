description(
"Checks that increased aggressiveness in sparse conditional constant propagation resultin from a node being proven to be force exit does not lead to a cascade of unsound decisions."
);

function foo(a) {
    var i;
    if (a.push)
        i = 0;
    else
        i = a;
    var result = 0;
    while (i < 10) {
        result += a[i];
        i++;
    }
    return result;
}

var array = [54, 5432, 1234, 54, 1235, 64, 75, 532, 64, 2];

for (var i = 0; i < 200; ++i)
    shouldBe("foo(array)", "8746");

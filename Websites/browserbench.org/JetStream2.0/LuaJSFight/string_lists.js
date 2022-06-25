var n = 40000;
var i = 0;
var items = [];

function run()
{
    items = [];
    while (i < n) {
        items.push("digit" + i);
        i = i + 1;
    }
}

class Benchmark {
    runIteration() {
        run();
    }
}

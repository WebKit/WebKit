
var obj = {
    "foo1": { "foo2": { "foo3":  { "foo4":  { "foo5":  { "foo6":  { "foo7": [
        { "bar1":  "a".repeat(670)},
        { "bar2":  "a".repeat(15771)},
    ]
    }}}}}}};

function doTest(x) {
    for (let i=1; i<10000; i++) {
        var s = JSON.stringify(x);
    }
}

doTest(obj);

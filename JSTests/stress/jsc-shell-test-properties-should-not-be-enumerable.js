//@ runDefault

function bar(o) {
    for (var x in o) {
        for(var y in o[x]) { }
    }
}

var foo = (function () {
    for (var prop in this) {
        bar(this[prop]);
    }
});

foo();

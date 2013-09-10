var global = [];

function foo(o, id) {
    o[id] = 42;
}

var sum = 0;
for (var i = 0; i < 20000; i++) {
    var o = {};
    foo(o, i);
    sum += o[i];
    global.push(o);
}

if (sum != 42 * 20000)
    throw "Error!"

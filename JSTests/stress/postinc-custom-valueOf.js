function postInc() {
    var counter = 0;
    var o = {};
    o.valueOf = () => {counter++; return 42};
    var x = o++; // The "var x =" part is required, or this gets transformed into ++o super early
    if (counter != 1)
        throw "A post-increment called valueOf " + counter + " times instead of just once.";
}

function postDec() {
    var counter = 0;
    var o = {};
    o.valueOf = () => {counter++; return 42};
    var x = o--; // The "var x =" part is required, or this gets transformed into --o super early
    if (counter != 1)
        throw "A pre-increment called valueOf " + counter + " times instead of just once.";
}

for (var i = 0; i < 20000; ++i) {
    postInc();
    postDec();
}

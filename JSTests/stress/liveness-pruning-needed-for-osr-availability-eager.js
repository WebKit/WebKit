// Note that this only fails in eager compilation.

function each(ary, func) {
    if (ary)
        for (var i = 0; i < ary.length && (!ary[i] ||!func(ary[i], i, ary)); i += 1);
}

var blah = function () {
    var func = function() {
        return (function () { }).apply(Object, arguments);
    };
    each([ {}, {} ], func);
};

for (var i = 0; i < 1000; i++)
    blah();

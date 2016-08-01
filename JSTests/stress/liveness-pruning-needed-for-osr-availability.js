function each(ary, func) {
    for (var i = 0; i < ary.length && (!ary[i] ||!func(ary[i], i, ary)); i += 1);
}

function foo() {
    each(
        [ {}, {} ],
        function () {
            return (function (x) { })(arguments);
        });
};
noInline(foo);

for (var i = 0; i < 100000; i++)
    foo();

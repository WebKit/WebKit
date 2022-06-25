function fn() {}
noInline(Function.prototype.apply);

for (var i = 0; i < 1e7; ++i) {
    fn.apply(null, []);
    fn.apply(null, [1,]);
    fn.apply(null, [1, 2,]);
}

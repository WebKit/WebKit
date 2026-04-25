function fn0() {}
function fn1(a) {}
function fn2(a, b) {}
function fn3(a, b, c) {}

for (var i = 0; i < 1e5; ++i) {
    fn0.bind();
    fn1.bind(null, 1);
    fn2.bind(null, 2);
    fn3.bind(null, 1, 2, 3);
}

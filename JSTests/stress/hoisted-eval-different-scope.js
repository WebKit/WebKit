for (var i = 0; i < 1e5; ++i) {
    eval(`
    var x = 1;
    with ({ g() { } }) {
        if (true) {
            function f() {
            }
        }
        f();
        g('here')
    }
    `)
}

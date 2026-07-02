for (var i = 0; i < testLoopCount; ++i) {
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

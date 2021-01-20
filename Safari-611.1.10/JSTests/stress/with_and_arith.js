for (var i = 0; i < 10000;) {
    var x = 1;
    with({}) {
        i += x;
    }
}

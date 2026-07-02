for (var i = 0; i < testLoopCount;) {
    var x = 1;
    with({}) {
        i += x;
    }
}

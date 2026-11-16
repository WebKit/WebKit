function Inner() {
    this.i = 0;
    this.doStuff = function() {
        this.i++;
        if (this.i > 10000)
            this.isDone();
    }
}

var foo = function() {
    var inner = new Inner();
    var done = false;
    inner.isDone = function() {
        done = true;
    }

    while (true) {
        var val = inner.doStuff();
        if (done)
            break;
    }
}

foo();

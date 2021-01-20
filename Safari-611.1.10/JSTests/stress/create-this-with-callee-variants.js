function createInLoop(x, count) {
    noInline(x)
    for (var i = 0; i < 5000; i++) {
        var obj = new x;
        if (!(obj instanceof x))
            throw "Failed to instantiate the right object";
    }
}

function y() { return function () {} }

createInLoop(y());

function z() { return function () {} }

createInLoop(z());
createInLoop(z());
createInLoop(z());

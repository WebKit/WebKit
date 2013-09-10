function f()
{
    var i;
    var limit = 150000;

    for (i = 0; (i < limit) == true; ++i) {
    }

    if (i != limit)
        throw "bad result";

    for (i = 0; (i < limit) === true; ++i) {
    }

    if (i != limit)
        throw "bad result";

    i = 0;
    for (var done = false; done == false; ) {
        if (!(++i < limit))
            done = true;
    }

    if (i != limit)
        throw "bad result";

    i = 0;
    while (true) {
        if ((++i < limit) == false)
            break;
    }

    if (i != limit)
        throw "bad result";

    i = 0;
    while (1) {
        if ((++i < limit) != true)
            break;
    }

    if (i != limit)
        throw "bad result";

    i = limit;
    while (--i) {
        if ((i & 1) == 0)
            continue;
    }

    if (i != 0)
        throw "bad result";
}

function g(x, y)
{
    var i;
    var limit = 150000;

    for (i = 0; i < limit; ++i) {
        if (true == false)
            break;
        if (true != true)
            break;
        if ("start" === "end")
            break;
        if (null !== null)
            break;
    }

    if (i != limit)
        throw "bad result";

    for (i = 0; i < limit; ++i) {
        if (x == false)
            break;
        if (x !== true)
            break;
        if (x != y)
            break;
        if (x !== y)
            break;
        x = x == y;
    }

    if (i != limit)
        throw "bad result";
}

f();
g(true, true);

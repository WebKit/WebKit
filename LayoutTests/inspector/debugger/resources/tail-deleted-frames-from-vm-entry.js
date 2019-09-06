"use strict";
function timeout(foo = 25) {
    return bar();
}
function bar(i = 9) {
    if (i > 0)
        return bar(i - 1);
    return 25;
}

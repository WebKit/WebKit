noInline(Object.keys);

function getArgs(a, b, c) {
    "use strict";
    return arguments;
}

for (var i = 0; i < 2e5; ++i)
    Object.keys(getArgs(1, 2, 3));

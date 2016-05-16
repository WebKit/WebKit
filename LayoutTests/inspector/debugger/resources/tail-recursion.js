"use strict";
function recurse(i) {
    if (i <= 0) {
        i++;
        i++;
        return;
    }

    return recurse(i - 1);
}

function startRecurse() {
    recurse(1000);
}

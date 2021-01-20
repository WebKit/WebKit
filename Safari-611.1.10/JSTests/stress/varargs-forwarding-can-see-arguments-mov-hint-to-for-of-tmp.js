// This should not crash.

let iter = {
    [Symbol.iterator]: () => {
        return { 
            next: function () {
                return arguments;
            }
        };
    }
}

let i = 0;
for (let x of iter) {
    i++;
    if (i >= 4000)
        break;
}

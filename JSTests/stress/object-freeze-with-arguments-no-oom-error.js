function foo(a) {
    Object.freeze(arguments);
}

let s = [0.1].toLocaleString().padEnd(2 ** 31 - 1, 'ab');
foo(s);

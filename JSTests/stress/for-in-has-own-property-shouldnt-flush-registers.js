function foo(o) {
    for (let p in o) {
        o.hasOwnProperty(p);
        o.__proto__ = undefined;
    }
}

for (let i = 0; i < 100000; ++i) {
    foo({f:42});
}


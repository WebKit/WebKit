function foo(a) {
    return [...a];
}
noInline(foo);

let arr = [];
for (let i = 0; i < 10000; i++) {
    if (i % 100 === 0)
        arr.push([], i);
    foo(arr);
}

let calledIterator = false;
Array.prototype[Symbol.iterator] = function iterator() {
    calledIterator = true;
    return {
        next() {
            return {done: true};
        }
    };
};

foo(arr);
if (!calledIterator)
    throw new Error("Bad result");

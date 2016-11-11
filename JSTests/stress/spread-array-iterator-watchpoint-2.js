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
let arrayIterator = [][Symbol.iterator]().__proto__;
arrayIterator.next = function() {
    calledIterator = true;
    return {done: true};
};

let r = foo(arr);
if (!calledIterator || r.length)
    throw new Error("Bad result");

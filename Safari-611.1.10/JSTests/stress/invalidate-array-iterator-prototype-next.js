Array.prototype[Symbol.iterator]().__proto__.next = 0;

let arr = [1, 2, 3];

try {
    for (let ele of arr) {
        throw new Error("It should never execute");
    }
} catch(e) {
    if (!e instanceof TypeError)
        throw new Error("It should throw a TypeError, but it threw " + e);
}


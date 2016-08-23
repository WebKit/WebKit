Object.defineProperty(Array.prototype, "sort", { writable: false, value: Array.prototype.sort });

function test(array) {
    array = array.splice(2, 2);
    array = array.slice(0, 5);
    array = array.concat([1,2,3]);
    return array;
}
noInline(test);

for (let i = 0; i < 100000; i++)
    test([1,2,3,4,5,6,7,8,9]);

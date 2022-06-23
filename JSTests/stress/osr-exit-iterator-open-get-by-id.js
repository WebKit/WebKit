var flag = 0;
flag = 1;
flag = 0;
var iterator = {
    nextImpl() {
        return { value: 42, done: true };
    },

    get next() {
        if (flag)
            OSRExit();
        return this.nextImpl;
    }
};


var object = {
    [Symbol.iterator]() {
        return iterator;
    }
};

noDFG(Object.getOwnPropertyDescriptor(object, Symbol.iterator).value);

function test()
{
    for (let i of object);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
flag = 1;
for (var i = 0; i < 1e6; ++i)
    test();

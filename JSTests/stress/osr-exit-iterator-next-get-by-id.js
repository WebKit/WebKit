var flag = 0;

var result = {
    get value() {
        return 42;
    },
    get done() {
        if (flag)
            OSRExit();
        return true;
    },
};

var iterator = {
    next() {
        return result;
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

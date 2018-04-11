function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var iterator;

var a = [];

function* foo(index) {
    while (1) {
        var q = a.pop();
        if(q){
            q.__proto__ = iterator;
            q.next();
        }
        yield index++;
    }
}

function* foo2(){
    yield;
}

var temp = foo2(0);

for(var i = 0; i < 10; i++) { // make a few objects with @generatorState set
    var q = {};
    q.__proto__ = temp;
    shouldThrow(() => {
        q.next();
    }, `TypeError: |this| should be a generator`);
    q.__proto__ = {};
    a.push(q);

}

iterator = foo(0);

var q = {};
q.__proto__ = iterator;
shouldThrow(() => {
    q.next();
}, `TypeError: |this| should be a generator`);

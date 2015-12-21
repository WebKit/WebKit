var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var functionConstructor = function () {
    this.func = () => this;
};

var instance = new functionConstructor();

testCase(instance.func() === instance, true, "Error: this is not lexically binded inside of the arrow function #2");

var obj = {
    method: function () {
        return () => this;
    }
};

noInline(obj.method);

for (var i=0; i < 10000; i++) {
    testCase(obj.method()() === obj, true, "Error: this is not lexically binded inside of the arrow function #3");
}

var fake = {steal: obj.method()};
noInline(fake.steal);

for (var i=0; i < 10000; i++) {
    testCase(fake.steal() === obj, true, "Error: this is not lexically binded inside of the arrow function #4");
}

var real = {borrow: obj.method};
noInline(real.borrow);

for (var i=0; i < 10000; i++) {
    testCase(real.borrow()() === real, true, "Error: this is not lexically binded inside of the arrow function #5");
}

// Force create the lexical env inside of arrow function

var functionConstructorWithEval = function () {
    this._id = 'old-value';
    this.func = () => {
        var f;
        eval('10==10');
        this._id = 'new-value';
        return this._id;
    };
};

var arrowWithEval = new functionConstructorWithEval();

for (var i=0; i < 10000; i++) {
    testCase(arrowWithEval.func() === 'new-value', true, "Error: this is not lexically binded inside of the arrow function #6");
}

function foo() {
    let arr = () => {
        var x = 123;
        function bas() {
            return x;
        };
        this._id = '12345';
        return bas();
    };
    this.arr = arr;
};

var fooObject = new foo();

for (var i=0; i < 10000; i++) {
    testCase(fooObject.arr() === 123, true, "Error: this is not lexically binded inside of the arrow function #7");
    testCase(fooObject._id === '12345', true, "Error: this is not lexically binded inside of the arrow function #8");
}

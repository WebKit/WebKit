description('Tests for ES6 arrow function lexical bind of this');

function Dog(name) {
    this.name = name;
    this.getName = () => eval("this.name");
    this.getNameHard = () => eval("(() => this.name)()");
    this.getNameNesting = () => () => () => this.name;
}

var d = new Dog("Max");
shouldBe('d.getName()', 'd.name');
shouldBe('d.getNameHard()', 'd.name');
shouldBe('d.getNameNesting()()()', 'd.name');

var obj = {
    name   : 'objCode',
    method : function () {
        return (value) => this.name + "-name-" + value;
    }
};

shouldBe("obj.method()('correct')", "'objCode-name-correct'");
obj.name='newObjCode';
shouldBe("obj.method()('correct')", "'newObjCode-name-correct'");

var deepObj = {
    name : 'wrongObjCode',
    internalObject: {
        name  :'internalObject',
        method: function () { return (value) => this.name + "-name-" + value; }
    }
};
shouldBe("deepObj.internalObject.method()('correct')", "'internalObject-name-correct'");

deepObj.internalObject.name = 'newInternalObject';

shouldBe("deepObj.internalObject.method()('correct')", "'newInternalObject-name-correct'");

var functionConstructor = function () {
    this.func = () => this;
};

var instance = new functionConstructor();

shouldBe("instance.func() === instance", "true");

var ownerObj = {
    method : function () {
        return () => this;
    }
};

shouldBe("ownerObj.method()() === ownerObj", "true");

var fake = { steal : ownerObj.method() };
shouldBe("fake.steal() === ownerObj", "true");

var real = { borrow : ownerObj.method };
shouldBe("real.borrow()() === real", "true");

var arrowFunction = { x : "right", getArrowFunction : function() { return z => this.x + z; }}.getArrowFunction();
var hostObj = { x : "wrong", func : arrowFunction };

shouldBe("arrowFunction('-this')", '"right-this"');
shouldBe("hostObj.func('-this')", '"right-this"');

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

shouldBe("arrowWithEval.func()", '"new-value"');

var internal_value_1 = 123;
var internal_value_2 = '1234';

function foo() {
    let arr = () => {
        var x = internal_value_1;
        function bas() {
            return x;
        };
        this._id = internal_value_2;
        return bas();
    };
    this.arr = arr;
};

var fooObject = new foo();

shouldBe("fooObject.arr()", 'internal_value_1');
shouldBe("fooObject._id", 'internal_value_2');

function boo() {
    let arr = () => {
        with ({'this': 40}) {
            return this;
        }
    }
    return arr();
}

let o1 = {'this': 20};

shouldBe('boo.call(o1)', 'o1');

var successfullyParsed = true;

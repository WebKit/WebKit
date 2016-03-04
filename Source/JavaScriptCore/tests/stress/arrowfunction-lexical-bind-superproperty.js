var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var testValue  = 'test-value';

var A = class A {
    constructor() {
        this.value = testValue;
    }
    getConstValue () {
        return testValue;
    }
    getValue () {
        return this.value;
    }
    setValue (value) {
        this.value = value;
    }
};

var B = class B extends A {
    getParentValue() {
        var arrow  = () => super.getValue();
        return arrow();
    }
};

var C = class C {
    constructor() {
        this.value = testValue;
    }
    static getStaticValue() {
        return testValue;
    }
};

var D = class D extends C {
    static getParentStaticValue() {
        var arrow  = () => super.getStaticValue();
        return arrow();
    }
};

var E = class E extends A {
     constructor() {
         super();
     }
     get prop() {
         var arrow = () => super.getConstValue() + '-' + this.value;
         return arrow();
     }
     set prop(value) {
         var arrow = (newVal) => super.setValue(newVal);
         arrow(value);
     }
     setInitValue() {
       this.value = testValue;
     }
 };

var b = new B();
for (var i = 0; i < 10000; i++) {
    testCase(b.getParentValue(), testValue, i);
}

for (var i = 0; i < 10000; i++) {
    testCase(D.getParentStaticValue(), testValue, i);
}

var e = new E();
for (var i = 0; i < 10000; i++) {
     e.setInitValue();
     testCase(e.prop, testValue+'-'+testValue, i);
     e.prop = 'new-test-value';
     testCase(e.prop, testValue+'-new-test-value', i);
}

var F  = class F extends A {
    newMethod() {
        var arrow  = () => eval('super.getValue()');
        var r = arrow();
        return r;
    }
};

var f = new F();
for (var i=0; i < 10000; i++) {
    try {
        var result = f.newMethod();
        testCase(result, testValue, i);
     } catch(e) {
        if (!(e instanceof SyntaxError))
           throw e;
     }
}

var G = class G extends A {
     constructor() {
         super();
     }
     get prop() {
         var arrow = () => () => super.getConstValue() + '-' + this.value;
         return arrow()();
     }
     set prop(value) {
         var arrow =  () => (newVal) => this.value = newVal;
         arrow()(value);
     }
     setInitValue() {
         this.value = testValue;
     }
     getValueCB() {
         var arrow  = () => super.getValue();
         return arrow;
     }
     setValueCB() {
         var arrow =  (newVal) => this.value = newVal;
         return arrow;
     }
     getParentValue() {
         return super.getValue();
     }

     getValueBlockScope() {
         if (true) {
             var someValue ='';
             if (true) {
                 return () => {
                    if (true) {
                        let internalValue = '';
                        return super.getValue();
                    }
                 }
             }
         }
     }
     *genGetParentValue() {
         let arr = () => super.getValue();
         yield arr();
     }
     *genGetParentValueDeepArrow() {
         let arr = () => () => () => super.getValue();
         yield arr()()();
     }
 };

 var g = new G();
 for (var i = 0; i < 10000; i++) {
    g.setInitValue();
    testCase(g.prop, testValue + '-' + testValue, 'Error: Some problem with using arrow and "super" inside of the method');
    g.prop = 'new-test-value';
    testCase(g.prop, testValue + '-new-test-value', 'Error: Some problem with using arrow and "super" inside of the getter and setter');
 }

var g1 = new G();
for (var i = 0; i < 10000; i++) {
    g1.setInitValue();
    let getValue = g1.getValueCB();
    testCase(getValue(), testValue,  'Error: Some problem with using arrow and "super" inside of the method that retun arrow function');
    let setValue = g1.setValueCB();
    setValue('new-value');
    testCase(getValue(), 'new-value', 'Error: Some problem with using arrow and "super" inside of the method that retun arrow function');
    getValue = g1.getValueBlockScope();
    testCase(getValue(), 'new-value',  'Error: Some problem with using arrow and "super" with deep nesting inside of the method that retun arrow function');
    testCase(g1.genGetParentValue().next().value, 'new-value',  'Error: Some problem with using arrow and "super" with deep nesting inside of the generator method that retun arrow function');
    testCase(g1.genGetParentValueDeepArrow().next().value, 'new-value',  'Error: Some problem with using arrow and "super" with deep nesting inside of the generator method that retun arrow function');
}

var H = class H extends A {
    constructor() {
        var arrow = () => () => super.getValue();
        super();
        this.newValue  = arrow()();
    }
};

for (var i = 0; i < 10000; i++) {
    let h = new H();
    testCase(h.newValue, testValue, 'Error: Some problem with using "super" inside of the constructor');
}

var I = class I extends A {
    constructor (beforeSuper) {
        var arrow = () => super.getValue();
        if (beforeSuper)  {
            this._value = arrow();
            super();
        } else {
            super();
            this._value = arrow();
        }
    }
}

var J = class J extends A {
    constructor (beforeSuper) {
        var _value;
        var arrow = () => super.getConstValue();
        if (beforeSuper)  {
            _value = arrow();
            super();
         } else {
            super();
            _value = arrow();
        }
        this._value = _value;
    }
}

for (var i = 0; i < 10000; i++) {
    let i = new I(false);
    testCase(i._value, testValue, 'Error: Some problem with using "super" inside of the constructor');
    let j = new J(false);
    testCase(j._value, testValue, 'Error: Some problem with using "super" inside of the constructor');

    // FIXME: Problem with access to the super before super() in constructor
    // https://bugs.webkit.org/show_bug.cgi?id=152108
    //let j2 = new J(true);
    //testCase(j2._value, testValue, 'Error: Some problem with using "super" inside of the constructor');
    error = false;
    try {
        new I(true);
    } catch (e) {
        error = e instanceof ReferenceError;
    }
    testCase(error, true, 'Error: using "super" property before super() should lead to error');
}

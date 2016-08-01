const oneCallOfParentConstructor = 1;
const twoCallOfParentConstructor = 2;

function tryCatch(klass) {
  let result = false;
  try {
    new klass();
  } catch(e) {
    result = e instanceof ReferenceError;
  }
  return result;
}

var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

let count = 0;
class A {
  constructor() {
    this.id = 0;
    count++;
  }
}

class B extends A {
  constructor() {
    super();
    super();
    super();
  }
}

testCase(tryCatch(B), true, 'Error: ReferenceError was not raised in case of two or more call super() #1');
testCase(count, twoCallOfParentConstructor, 'Excpected two call of parent constructor #1');

count = 0;
class C extends A {
  constructor() {
    (()=>super())();
    (()=>super())();
    (()=>super())();
  }
}

testCase(tryCatch(C), true, 'Error: ReferenceError was not raised in case of two or more call super() in arrrow function #2');
testCase(count, twoCallOfParentConstructor, 'Excpected two call of parent constructor in arrow function #2');

count = 0;
class D extends A {
  constructor() {
    eval('super()');
    eval('super()');
    eval('super()');
  }
}

testCase(tryCatch(D), true, 'Error: ReferenceError was not raised in case of two or more call super() in eval #3');
testCase(count, twoCallOfParentConstructor, 'Excpected two call of parent constructor in eval #3');

count = 0;
class E extends A {
  constructor() {
    (()=>eval('super()'))();
    (()=>eval('super()'))();
    (()=>eval('super()'))();
  }
}

testCase(tryCatch(E), true, 'Error: ReferenceError was not raised in case of two or more call super() in eval within arrow function #4');
testCase(count, twoCallOfParentConstructor, 'Excpected two call of parent constructor in eval within arrow function #4');

count = 0;
class F extends A {
    constructor() {
        super();
        var arrow = () => 'testValue';
        arrow();
    }
}

testCase(tryCatch(F), false, 'Error: ReferenceError was raised but should not be #5');
testCase(count, oneCallOfParentConstructor, 'Excpected two call of parent constructor #5');

count = 0;
class G extends A {
    constructor() {
        super();
        eval('(()=>"abc")()');
    }
}

testCase(tryCatch(G), false, 'Error: ReferenceError was raised but should not be #6');
testCase(count, oneCallOfParentConstructor, 'Excpected two call of parent constructor #6');

count = 0;
class H extends A {
    constructor() {
        eval('(()=>eval("super()"))()');
        try {
            eval('(()=>eval("super()"))()');
        } catch(e) {
          let result = e instanceof ReferenceError;
          if (!result) throw new Error('Wrong type error');
        }
        try {
            eval('(()=>eval("super()"))()');
          } catch(e) {
            let result = e instanceof ReferenceError;
            if (!result) throw new Error('Wrong type error');
          }
        try {
            eval('(()=>eval("super()"))()');
        } catch(e) {
            let result = e instanceof ReferenceError;
            if (!result) throw new Error('Wrong type error');
        }
    }
}

testCase(tryCatch(H), false, 'Error: ReferenceError was raised but should not be #7');
testCase(count, 4, 'Excpected two call of parent constructor #7');

noInline(B);
for (var i = 0; i < 10000; i++) {
    count = 0;
    let result = false;
    try {
        new B();
    } catch(e) {
        result = e instanceof ReferenceError;
    }

    testCase(result, true, '');
    testCase(count, 2, '');
}

count = 0;
class I extends A {
  constructor() {
    super();
    (()=>super())();
  }
}

testCase(tryCatch(I), true, 'Error: ReferenceError was not raised in case of two or more call super() #8');
testCase(count, 2, 'Excpected two call of parent constructor #8');

count = 0;
class J extends A {
  constructor() {
    super();
    eval('super()');
  }
}

testCase(tryCatch(J), true, 'Error: ReferenceError was not raised in case of two or more call super() #9');
testCase(count, 2, 'Excpected two call of parent constructor #9');

let maxCount = 150000;
class K extends A {
  constructor(i) {
    if (i % 2 === 0 )
      super();
    if (i % 2 !== 0 || maxCount === i)
      super();
  }
}

noInline(K);
let result = false;
try {
    count = 0;
    for (var i = 1; i <= maxCount; i++) {
        new K(i);
    }
} catch (e) {
    result = e instanceof ReferenceError;
}
testCase(result, true, 'Error: ReferenceError was not raised in case of two or more call super() #10');
testCase(count, maxCount + 1, 'Excpected a lot of calls of parent constructor #10');

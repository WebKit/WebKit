description('Tests for ES6 arrow function lexical bind of arguments');

function afFactory0() {
    return a => arguments;
}
var af0 = afFactory0('ABC', 'DEF');

var arr = af0(0);

shouldBe('arr.length', '2');
shouldBe('arr[0]','"ABC"');
shouldBe('arr[1]','"DEF"');
shouldBe('typeof arr[2]','"undefined"');

function afFactory1(x, y, z) {
    return (a, b) => arguments[0] + '-' + arguments[1] + '-' + arguments[2] + '-' + a + '-' + b;
}

shouldBe("afFactory1('AB', 'CD', 'EF')('G', 'H')", '"AB-CD-EF-G-H"');

var afFactory2 = function () {
    this.func = (a, b) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b;
};

shouldBe("(new afFactory2('P1', 'Q2', 'R3')).func('A4', 'B5')", '"P1_Q2_R3_A4_B5"');

var afFactory3 = function () {
    this.func = (a, b) => (c, d) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b + '_' + c + '_' + d;
};

shouldBe("(new afFactory3('PQ', 'RS', 'TU')).func('VW', 'XY')('Z', 'A')", '"PQ_RS_TU_VW_XY_Z_A"');

var afNested = function () {
    return function () {
        this.func = (a, b) => (c, d) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b + '_' + c + '_' + d;
    };
};

var afInternal = new afNested('AB', 'CD', 'EF');
var af5 = new afInternal('GH', 'IJ', 'KL');
shouldBe("af5.func('VW', 'XY')('Z', '')", '"GH_IJ_KL_VW_XY_Z_"');

var objFactory = function () {
    return {
        name : 'nested',
        method : (index) => arguments[0] + '-' + index
    };
};

var objInternal = objFactory('ABC', 'DEF');
shouldBe("objInternal.method('H')", '"ABC-H"');

shouldBe("(() => arguments)()", "arguments")

var func_with_eval = function (a, b) { return () => eval('arguments') }

shouldBe('func_with_eval("abc", "def")("xyz")[0]', '"abc"');
shouldBe('func_with_eval("abc", "def")("xyz")[1]', '"def"');

var af_function_scope = function (first, x, y) {
    let arr;
    var arguments = 'af_function_scope';
    if (first) {
        arr = () => arguments;
    } else {
        arr = () => arguments;
    }
    return arr;
};

var af_mixed_scope = function (first, x, y) {
    let arr;
    var arguments = 'af_mixed_scope';
    if (first) {
        let arguments = 'local-scope';
        arr = () => arguments;
    } else {
        let arguments = 'local-scope-2';
        arr = () => arguments;
    }
    return arr;
};

var af_block_scope = function (x, y) {
    let arr;
    if (true) {
        let arguments = 'branch-1';
        arr = () => arguments;
    } else {
        let arguments = 'branch-2';
        arr = () => arguments;
    }
    return arr;
};

shouldBe("af_block_scope('A', 'B')('C')", "'branch-1'");
shouldBe("af_function_scope(true, 'D', 'E')('F')", "'af_function_scope'");
shouldBe("af_mixed_scope(true, 'G', 'H')('I')", "'local-scope'");

class A {
   constructor() {
      this.list = [];
   }
};

class B extends A {
   addObj(obj) {
      this.list.push(obj);
      this.result = 0;
   }
   runAll() {
      for (let i = 0; i < this.list.length; i++) {
          this.result += this.list[i].operand(1);
      }
   }
};

function test() {
    let b = new B();

    function runTest () {
        b.addObj({ operand : (value) =>  value + value });
        b.addObj({ operand : (value) =>  value + value });
    }

    for (var i = 0; i < 10000; i++) {
        runTest();
    }

    b.runAll();

    return b.result;
}

test();

shouldBe("test()", "40000");

function* foo(a, b, c) {
    yield () => arguments;
}

foo(10, 11, 12).next().value()[0];

shouldBe("foo(10, 11, 12).next().value()[0]", "10");
shouldBe("foo(10, 11, 12).next().value()[1]", "11");
shouldBe("foo(10, 11, 12).next().value()[2]", "12");

var successfullyParsed = true;

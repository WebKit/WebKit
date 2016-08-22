var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var txtMsg = 'Error: arguments is not lexically binded inside of the arrow function ';

function afFactory0() {
    return a => arguments;
}

var af0 = afFactory0('ABC', 'DEF');

noInline(af0);

for (var i=0; i<10000; i++) {
    var arr = af0(i);

    testCase(arr.length, 2, txtMsg + "#1");
    testCase(arr[0],'ABC', txtMsg + "#2");
    testCase(arr[1],'DEF', txtMsg + "#3");
    testCase(typeof arr[2], 'undefined',  txtMsg + "#4");
}


function afFactory() {
    return a => arguments[0];
}

var af = afFactory(12);

noInline(af);

for (var i=0; i<10000; i++) {
    testCase(af(6), 12, txtMsg + "#5");
}

function afFactory1(x, y, z) {
    return (a, b) => arguments[0] + '-' + arguments[1] + '-' + arguments[2] + '-' + a + '-' + b;
}

var af1 = afFactory1('AB', 'CD', 'EF');

noInline(af1);

for (var i = 0; i < 10000; i++) {
    testCase(af1('G', i), 'AB-CD-EF-G-' + i, txtMsg + "#5");
}

if (true) {
    let arguments = [];

    var af2 = (x, y) => arguments[0] + '-' + x + y;

    noInline(af2);

    for (var i = 0; i < 10000; i++) {
        testCase(af2('ABC', i), 'undefined-ABC' + i, txtMsg + "#6");
    }

    var af3 = () => arguments;

    noInline(af3);

    for (var i = 0; i < 10000; i++) {
        testCase(typeof af3('ABC', i), 'object', txtMsg + "#7");
        testCase(typeof af3('ABC', i)[0], 'undefined', txtMsg + "#8");
    }
}

var afFactory4 = function () {
    this.func = (a, b) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b;
};

var af4 = new afFactory4('P1', 'Q2', 'R3');
noInline(af4);

for (var i = 0; i < 10000; i++) {
    testCase(af4.func('EF', i), 'P1_Q2_R3_EF_' + i, txtMsg + "#9");
}

var afFactory5 = function () {
    this.func = (a, b) => (c, d) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b + '_' + c + '_' + d;
};

var af5 = new afFactory5('PQ', 'RS', 'TU');
noInline(af5);

for (var i = 0; i < 10000; i++) {
    testCase(af5.func('VW', 'XY')('Z',i), 'PQ_RS_TU_VW_XY_Z_' + i, txtMsg + "#9");
}

var afNested = function () {
    return function () {
        this.func = (a, b) => (c, d) => arguments[0] + '_' + arguments[1] + '_' + arguments[2] + '_' + a + '_' + b + '_' + c + '_' + d;
    };
};

var afInternal = new afNested('AB', 'CD', 'EF');
var af6 = new afInternal('GH', 'IJ', 'KL');
noInline(af6);

for (var i = 0; i < 10000; i++) {
    testCase(af6.func('VW', 'XY')('Z',i), 'GH_IJ_KL_VW_XY_Z_' + i, txtMsg + "#9");
}

if (true) {
    let arguments = [];

    var obj = {
        name : 'id',
        method : (index) => arguments[0] + '-' + index
    };

    noInline(obj.method);

    for (var i = 0; i < 10000; i++) {
        testCase(obj.method(i), 'undefined-' + i, txtMsg + "#10");
    }
}

var objFactory = function () {
    return {
        name : 'nested',
        method : (index) => arguments[0] + '-' + index
    };
};

var objInternal = objFactory('ABC', 'DEF');

for (var i = 0; i < 10000; i++) {
    testCase(objInternal.method(i), 'ABC-' + i, txtMsg + "#11");
}

var af_block_scope = function (first, x, y) {
    let arr;
    if (first) {
        let arguments = 'branch-1';
        arr = () => arguments;
    } else {
        let arguments = 'branch-2';
        arr = () => {
            if (true) {
                let arguments = 'internal-arrow-block-scope';
                return arguments;
            }
        };
    }
    return arr;
};

var af_function_scope = function (first, x, y) {
    let arr;
    var arguments = 'af_function_scope';
    if (first) {
        arr = () => arguments;
    } else {
        arr = () => {
         var arguments = 'internal-arrow-scope';
         return arguments;
        };
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
        arr = () => {
            let arguments = 'internal-arrow-scope';
            return arguments;
        };
    }
    return arr;
};

for (var i = 0; i < 10000; i++) {
    testCase(af_block_scope(true, 'A', 'B')('C'), 'branch-1', txtMsg + "#12");
    testCase(af_block_scope(false, 'A', 'B')('C'), 'internal-arrow-block-scope', txtMsg + "#12");
    testCase(af_function_scope(true, 'D', 'E')('F'), 'af_function_scope', txtMsg + "#13");
    testCase(af_function_scope(false, 'D', 'E')('F'), 'internal-arrow-scope', txtMsg + "#13");
    testCase(af_mixed_scope(true, 'G', 'H')('I'), 'local-scope', txtMsg + "#14");
    testCase(af_mixed_scope(false, 'G', 'H')('I'), 'internal-arrow-scope', txtMsg + "#14");
}

function foo() {
    var x = (p) => eval(p);
    return x;
}

var foo_arr = foo('A', 'B');

for (var i = 0; i < 10000; i++) {
    testCase(foo_arr('arguments[0]'), 'A', txtMsg + "#15");
    testCase(foo_arr('arguments[1]'), 'B', txtMsg + "#16");
}

function boo() {
    return () => {
        return () => {
            return function () {
                return () => arguments;
            }
        }
    }
}

for (var i = 0; i < 10000; i++) {
    testCase(boo('A' + i)('B' + i)('D' + i)('E' + i)('G' + i)[0], 'E' + i, txtMsg + "#17");
}

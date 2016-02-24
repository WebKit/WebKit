var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

let testValue = 'test-value';

var f_this = function () {
    let value = 'value';
    if (true) {
        let someValue = 'someValue';
        if (true) {
            let = anotherValue = 'value';
            return () => () => () => this.value;
        }
    }

    return () => value;
}

for (let i = 0; i < 10000; i++) {
    testCase(f_this.call({value : testValue})()()(), testValue);
}

var f_this_eval = function () {
    if (true) {
        let someValue = '';
        if (true) {
            let = anotherValue = 'value';
            return () => () => () => eval('this.value');
        }
    }

    return () => 'no-value';
}

for (let i = 0; i < 10000; i++) {
    testCase(f_this_eval.call({value : testValue}, false)()()(), testValue);
}


function f_this_branches (branch, returnThis) {
    let value = 'value';
    if (branch === 'A') {
        let someValue = 'someValue';
        if (true) {
            let = anotherValue = 'value';
            return () => () => () => {
                if (returnThis)
                    return this.value;
                  else
                    return anotherValue;
            }
        }
    }

    return () => value;
}

for (let i = 0; i < 10000; i++) {
    testCase(f_this_branches.call({value : testValue}, 'B')() == testValue, false);
    testCase(f_this_branches.call({value : testValue}, 'A', false)()()() == testValue, false);
    testCase(f_this_branches.call({value : testValue}, 'A', true)()()(), testValue);
}

function f_this_eval_branches (branch, returnThis) {
    let value = 'value';
    if (branch === 'A') {
        let someValue = 'someValue';
        if (true) {
            let = anotherValue = 'value';
            return () => () => () => {
                if (returnThis)
                    return eval('this.value');
                  else
                    return anotherValue;
            }
        }
    }

    return () => value;
}

for (let i = 0; i < 10000; i++) {
    testCase(f_this_eval_branches.call({value : testValue}, 'B')() == testValue, false);
    testCase(f_this_eval_branches.call({value : testValue}, 'A', false)()()() == testValue, false);
    testCase(f_this_eval_branches.call({value : testValue}, 'A', true)()()(), testValue);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

{
    let constructed = false;
    class MyRegExp extends RegExp {
        constructor(...args) {
            constructed = true;
            super(...args);
        }
    }
    let re = /,/;
    let obj = {
        toString() {
            re.constructor = MyRegExp;
            return "a,b,c";
        }
    };
    let result = String.prototype.split.call(obj, re);
    shouldBe(constructed, true);
    shouldBe(JSON.stringify(result), '["a","b","c"]');
}

// Overriding RegExp.prototype.exec fires the primordial RegExp watchpoint, so
// this must stay the last case in this file.
{
    let execCalled = false;
    let re = /,/;
    let obj = {
        toString() {
            RegExp.prototype.exec = function() {
                execCalled = true;
                return null;
            };
            return "a,b,c";
        }
    };
    let result = String.prototype.split.call(obj, re);
    shouldBe(execCalled, true);
    shouldBe(JSON.stringify(result), '["a,b,c"]');
}

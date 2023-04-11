//@ skip if $buildType == "debug"
//@ skip if $hostOS == "windows"

const startNumberOfParentheses = 1500;
const endNumberOfParentheses = 2000;
const incNumberOfParentheses = 500;

let numberOfParentheses = startNumberOfParentheses;

let regExSource;

function baz() {
    try {
        eval(`bar(${regExSource})`);
    } catch (error) {
        if (error.toString() != "RangeError: Maximum call stack size exceeded.")
            print("Expected \"RangeError: Maximum call stack size exceeded.\", Caught: \"" + error + "\"");
        if (numberOfParentheses >= endNumberOfParentheses)
            quit(0);
        throw "Next count";
    }
}

function bar(a0) {
    a0.test("abc");
}

function foo() {
    baz();
    foo();
}

while (true) {
    regExSource = `/${'('.repeat(numberOfParentheses)}${')'.repeat(numberOfParentheses)}/`;
    try {
        foo();
    } catch (e) {
        numberOfParentheses += incNumberOfParentheses;
    }
}

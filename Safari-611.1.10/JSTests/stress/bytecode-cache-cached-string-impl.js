//@ runBytecodeCache

function f() {
    let x = {};
    x.foo = "foo";
}
function g() {
    switch ('foo') {
        case "foo":
            break;
        case "bar":
        case "baz":
        default:
            throw new Error('Should have jumped to `foo`');
            break;
    }
}

g();
f();

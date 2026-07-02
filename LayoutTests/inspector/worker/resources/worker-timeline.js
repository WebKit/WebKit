function bar(x) {
    return x * 10;
}

function foo() {
    bar(42);
}

let customTarget = new EventTarget;
customTarget.addEventListener("custom", (event) => {
    foo();
});

function willCallFunctionTest() {
    console.profile();
    customTarget.dispatchEvent(new CustomEvent("custom"));
    console.profileEnd();
}

function willEvaluateScriptTest() {
    console.profile();
    foo();
    console.profileEnd();
}

self.addEventListener("message", (event) => {
    switch (event.data) {
    case "willCallFunctionTest":
        willCallFunctionTest();
        break;
    case "willEvaluateScriptTest":
        willEvaluateScriptTest();
        break;
    }
});

description(
"Tests that posting a message that consists of an object with a numeric property doesn't try to reify the property as a named property."
);

jsTestIsAsync = true;

window.onmessage = function(e) {
    var foo = e.data;
    if (foo[0] == "hi")
        testPassed("e.data[0] is 'hi'");
    else
        testFailed("e.data[0] is not 'hi'");
    finishJSTest();
}

window.postMessage({0:"hi"}, "*");

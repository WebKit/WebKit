var createDOMJITNodeObject = $vm.createDOMJITNodeObject;
var createDOMJITCheckJSCastObject = $vm.createDOMJITCheckJSCastObject;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var array = [];
for (var i = 0; i < 100; ++i)
    array.push(createDOMJITCheckJSCastObject());

// DOMJITNode is an instance of a super class (DOMJITNode) of DOMJITCheckJSCastObject.
var node = createDOMJITNodeObject();
node.func = createDOMJITCheckJSCastObject().func;

function calling(dom)
{
    return dom.func();
}
noInline(calling);

array.forEach((obj) => {
    shouldBe(calling(obj), 42);
});

shouldThrow(() => {
    calling(node);
}, `TypeError: Type error`);

for (var i = 0; i < 1e3; ++i) {
    array.forEach((obj) => {
        shouldBe(calling(obj), 42);
    });
}

shouldThrow(() => {
    calling(node);
}, `TypeError: Type error`);

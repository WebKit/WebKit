description(
"Tests that an event handler registered on two nodes still fires after one of the nodes has been GC'd."
);

function gc() {
    if (window.GCController)
        GCController.collect();
    else {
        for (var i = 0; i < 10000; ++i)
            new Object;
    }
}

var clickCount = 0;
function clickHandler() {
    ++clickCount;
};

(function () {
    // Register 'clickHandler' on some referenced divs.
    var divs = [];
    for (var i = 0; i < 100; ++i) {
        var div = document.createElement("div");
        div.addEventListener("click", clickHandler, false);
        divs.push(div);
    }
    
    // Register 'clickHandler' on some garbage divs.
    for (var i = 0; i < 100; ++i) {
        var div = document.createElement("div");
        div.addEventListener("click", clickHandler, false);
    }
    
    // GC the garbage divs.
    gc();
    
    for (var i = 0; i < 100; ++i) {
        var clickEvent = document.createEvent("MouseEvents");
        clickEvent.initMouseEvent("click", true, true, null, 1, 1, 1, 1, 1, false, false, false, false, 0, document);
        divs[i].dispatchEvent(clickEvent);
    }
    
    shouldBe("clickCount", "100");
})();

var successfullyParsed = true;

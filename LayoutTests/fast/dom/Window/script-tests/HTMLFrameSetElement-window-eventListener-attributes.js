description("This tests that setting window event listeners on the frameset, sets them on the window.");

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("");
    }
}

var frameSet = document.createElement("frameset");
var func = function() { }

frameSet.onblur = func;
shouldBe("window.onblur", "func");
shouldBe("window.onblur", "frameSet.onblur");

frameSet.onfocus = func;
shouldBe("window.onfocus", "func");
shouldBe("window.onfocus", "frameSet.onfocus");

frameSet.onerror = func;
shouldBe("window.onerror", "func");
shouldBe("window.onerror", "frameSet.onerror");

frameSet.onload = func;
shouldBe("window.onload", "func");
shouldBe("window.onload", "frameSet.onload");

frameSet.onbeforeunload = func;
shouldBe("window.onbeforeunload", "func");
shouldBe("window.onbeforeunload", "frameSet.onbeforeunload");

frameSet.onhashchange = func;
shouldBe("window.onhashchange", "func");
shouldBe("window.onhashchange", "frameSet.onhashchange");

frameSet.onmessage = func;
shouldBe("window.onmessage", "func");
shouldBe("window.onmessage", "frameSet.onmessage");

frameSet.onoffline = func;
shouldBe("window.onoffline", "func");
shouldBe("window.onoffline", "frameSet.onoffline");

frameSet.ononline = func;
shouldBe("window.ononline", "func");
shouldBe("window.ononline", "frameSet.ononline");

frameSet.onresize = func;
shouldBe("window.onresize", "func");
shouldBe("window.onresize", "frameSet.onresize");

frameSet.onstorage = func;
shouldBe("window.onstorage", "func");
shouldBe("window.onstorage", "frameSet.onstorage");

frameSet.onunload = func;
shouldBe("window.onunload", "func");
shouldBe("window.onunload", "frameSet.onunload");
window.onunload = null;

gc();

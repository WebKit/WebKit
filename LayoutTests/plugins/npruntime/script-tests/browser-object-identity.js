description("Test that plug-in doesn't get a new browser object instance each time")

function gc()
{
    if (window.GCController)
        return GCController.collect();

    for (var i = 0; i < 10000; i++) { // > force garbage collection (FF requires about 9K allocations before a collect)
        var s = new String("abc");
    }
}

embed = document.createElement("embed");
embed.type = "application/x-webkit-test-netscape";
document.body.appendChild(embed);

var obj = new XMLHttpRequest;
obj.foo = "bar";
embed.remember(obj);
obj = null;
gc();
shouldBe("embed.getAndForgetRememberedObject().foo", "'bar'");
gc();

obj = new XMLHttpRequest;
shouldBe("embed.refCount(obj)", "1");
shouldBe("embed.refCount(obj)", "1");
embed.remember(obj);
shouldBe("embed.refCount(obj)", "2");
shouldBe("embed.getRememberedObject()", "obj");
shouldBe("embed.getRememberedObject()", "obj");
shouldBe("embed.refCount(obj)", "2");
shouldBe("embed.getAndForgetRememberedObject()", "obj");
shouldBe("embed.refCount(obj)", "1");
obj = null;
gc();

var successfullyParsed = true;

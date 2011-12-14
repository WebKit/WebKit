function test()
{
    (new String("abc")) + (new Date).toGMTString() + null;
    new Array(new Number(0), 1, 2, 3);
    new RegExp("");
    "foo".match(/[a-z]+/);
    "a".localeCompare("A");
    try {
        throw 0;
    } catch (ex) {
    }
}

var postedMessage = false;
while (true) {
    try {
        test();
        eval("test()");
    } catch (ex) {
        if (!postedMessage) {
            postMessage("Unexpected exception: " + ex);
            postedMessage = true;
        }
    }
}

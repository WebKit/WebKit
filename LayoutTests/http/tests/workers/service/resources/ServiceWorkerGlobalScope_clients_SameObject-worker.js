function gc() {
    if (typeof GCController !== "undefined")
        GCController.collect();
    else {
        var gcRec = function (n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        };
        for (var i = 0; i < 1000; i++)
            gcRec(10);
    }
}

let client = null;

self.addEventListener("message", (event) => {
    client = event.source;
    self.clients.foo = 1;
    gc();
    setTimeout(function() {
        gc();
        if (self.clients.foo === 1)
            client.postMessage("PASS: returned object was the same");
        else
            client.postMessage("FAIL: returned object was not the same");
    }, 0);
});

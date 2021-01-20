onmessage = function(event) {
    switch (event.data) {
    case "log":
        console.log("log!", [self, self.location, 123, Symbol()]);
        break;
    case "warn":
        console.warn("warning!");
        break;
    case "error":
        console.error("error!");
        break;
    case "assert":
        console.assert(true, "SHOULD NOT SEE THIS");
        console.assert(false, "Assertion Failure");
        break;
    case "time":
        console.time("name");
        console.timeEnd("name");
        break;
    case "count":
        console.count();
        break;
    }
}

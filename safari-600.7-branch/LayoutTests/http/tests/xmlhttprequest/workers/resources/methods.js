importScripts("worker-pre.js");

function log(message)
{
    postMessage("log " + message);
}

function done()
{
    postMessage("DONE");
}

function init()
{
    // sync
    req = new XMLHttpRequest;
    req.open("GET", "methods.cgi", false);
    req.send("");
    log('GET(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("GET", "methods.cgi", false);
    req.send(null);
    log('GET(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("GET", "methods.cgi", false);
    req.send("123");
    log('GET("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("POST", "methods.cgi", false);
    req.send("");
    log('POST(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("POST", "methods.cgi", false);
    req.send(null);
    log('POST(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("POST", "methods.cgi", false);
    req.send("123");
    log('POST("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("PUT", "methods.cgi", false);
    req.send("");
    log('PUT(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("PUT", "methods.cgi", false);
    req.send(null);
    log('PUT(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("PUT", "methods.cgi", false);
    req.send("123");
    log('PUT("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("DELETE", "methods.cgi", false);
    req.send("");
    log('DELETE(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("DELETE", "methods.cgi", false);
    req.send(null);
    log('DELETE(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("DELETE", "methods.cgi", false);
    req.send("123");
    log('DELETE("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("HEAD", "methods.cgi", false);
    req.send("");
    log('HEAD(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("HEAD", "methods.cgi", false);
    req.send(null);
    log('HEAD(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("HEAD", "methods.cgi", false);
    req.send("123");
    log('HEAD("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("WKFOOBAR", "methods.cgi", false);
    req.send("");
    log('WKFOOBAR(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("WKFOOBAR", "methods.cgi", false);
    req.send(null);
    log('WKFOOBAR(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("WKFOOBAR", "methods.cgi", false);
    req.send("123");
    log('WKFOOBAR("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("SEARCH", "methods.cgi", false);
    req.send("");
    log('SEARCH(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("SEARCH", "methods.cgi", false);
    req.send(null);
    log('SEARCH(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));

    req.open("SEARCH", "methods.cgi", false);
    req.send("123");
    log('SEARCH("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
    done();
}

importScripts("worker-pre.js");

onmessage = function(evt)
{
    if (evt.data == "START")
        start();
}

function log(message)
{
    postMessage("log " + message);
}

function done()
{
    postMessage("DONE");
}

function onReqAbort()
{
    log('Abort event.');
}

function onSyncReqError()
{
    log('Error event.');
}

// async
    
var asyncStep = 1;

function start() 
{
    req = new XMLHttpRequest();
    req.onreadystatechange = processStateChange;
    req.onerror = onSyncReqError;
    req.onabort = onReqAbort;
    req.open("GET", "methods.cgi", true);
    req.send("");
}

function processStateChange()
{
    if (req.readyState == 4){
        if (req.status == 200){
            if (asyncStep == 1) {
                asyncStep = 2;
                log('GET(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("GET", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 2) {
                asyncStep = 3;
                log('GET(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("GET", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 3) {
                asyncStep = 4;
                log('GET("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("POST", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 4) {
                asyncStep = 5;
                log('POST(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("POST", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 5) {
                asyncStep = 6;
                log('POST(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("POST", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 6) {
                asyncStep = 7;
                log('POST("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("PUT", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 7) {
                asyncStep = 8;
                log('PUT(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("PUT", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 8) {
                asyncStep = 9;
                log('PUT(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("PUT", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 9) {
                asyncStep = 10;
                log('PUT("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("DELETE", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 10) {
                asyncStep = 11;
                log('DELETE(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("DELETE", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 11) {
                asyncStep = 12;
                log('DELETE(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("DELETE", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 12) {
                asyncStep = 13;
                log('DELETE("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("HEAD", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 13) {
                asyncStep = 14;
                log('HEAD(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("HEAD", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 14) {
                asyncStep = 15;
                log('HEAD(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("HEAD", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 15) {
                asyncStep = 16;
                log('HEAD("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("WKFOOBAR", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 16) {
                asyncStep = 17;
                log('WKFOOBAR(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("WKFOOBAR", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 17) {
                asyncStep = 18;
                log('WKFOOBAR(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("WKFOOBAR", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 18) {
                asyncStep = 19;
                log('WKFOOBAR("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("SEARCH", "methods.cgi", true);
                req.send("");
            } else if (asyncStep == 19) {
                asyncStep = 20;
                log('SEARCH(""): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("SEARCH", "methods.cgi", true);
                req.send(null);
            } else if (asyncStep == 20) {
                asyncStep = 21;
                log('SEARCH(null): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                req.open("SEARCH", "methods.cgi", true);
                req.send("123");
            } else if (asyncStep == 21) {
                log('SEARCH("123"): ' + req.getResponseHeader("REQMETHOD") + "(" + req.getResponseHeader("REQLENGTH") + " bytes), Content-Type: " + req.getResponseHeader("REQTYPE"));
                done();
            }
        } else {
            log("Error loading URL: status " + req.status);
            done();
        }
    }
}

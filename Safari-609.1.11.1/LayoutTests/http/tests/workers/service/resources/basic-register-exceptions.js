function done()
{
    finishSWTest();
}

var registrations = [
    ["image-mime-type.php", {}],
    ["", {}],
    ["http://127.0.0.1:abc", {}],
    ["somescheme://script.js", { }],
    ["resources/%2fscript.js", { }],
    ["resources/script.js", { scope: "somescheme://script.js" }],
    ["resources/script.js", { scope: "%2fscript.js" }],
    ["http://localhost:8000/workers/service/resources/empty-worker.js", { }],
    ["resources/empty-worker.js", { scope: "http://localhost:8000/workers/service/" }]
];

async function doTest()
{
    for (let registration of registrations) {
        await navigator.serviceWorker.register(registration[0], registration[1]).then(function(r) {
            console.log("Registered! (unexpectedly)");
        }, function(e) {
            console.log("Registration failed with error: " + e);
        }).catch(function(e) {
            console.log("Exception registering: " + e);i
        });
    }
    finishSWTest();
}

doTest();

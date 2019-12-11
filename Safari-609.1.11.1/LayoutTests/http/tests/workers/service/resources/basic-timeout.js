async function test()
{
    var urlResults = new Array;
    var statusResults = new Array;
    var headerResults = new Array;

    function finishThisTest()
    {
        for (var i = 0; i < urlResults.length; ++i) {
            log("URL " + i + " - " + urlResults[i]);
            log("Status code " + i + " - " + statusResults[i]);
            log("Source' header " + i + " - " + headerResults[i]);
        }
        finishSWTest();
    }
    
    try {
        var frame = await interceptedFrame("resources/basic-timeout-worker.js", "/workers/service/resources/");
        var fetch = frame.contentWindow.fetch;

        if (window.testRunner)
            testRunner.setServiceWorkerFetchTimeout(0);
        
        // The following two fetches should time out immediately
        fetch("timeout-fallback.html").then(function(response) {
            urlResults[0] = response.url;
            statusResults[0] = response.status;
            headerResults[0] = response.headers.get("Source");       
        }, function(error) {
            log(error);
        });

        fetch("timeout-no-fallback.html").then(function(response) {
            urlResults[1] = response.url;
            statusResults[1] = response.status;
            headerResults[1] = response.headers.get("Source");   
        }, function(error) {
            log(error);
        });

        if (window.testRunner)
            testRunner.setServiceWorkerFetchTimeout(60);

        // The service worker knows how to handle the following fetch *and* has 60 seconds to do so.
        // But will be cancelled with the above fetches since we're terminating the service worker, and 
        // therefore it will then fallback to the network.
        fetch("succeed-fallback-check.php").then(function(response) {
            urlResults[2] = response.url;
            statusResults[2] = response.status;
            headerResults[2] = response.headers.get("Source");   
            setTimeout(checkSuccessAgain, 0);
        }, function(error) {
            log(error);
            finishSWTest();
        });
        
        // Now we can fetch that same URL again, which *could* relaunch the service worker and handle it there, but for now this service worker registration is inert and fetches through it will go to the network instead.
        // I'm leaving this in to cover future cases where we do relaunch the SW to handle it.
        function checkSuccessAgain() {
            fetch("succeed-fallback-check.php").then(function(response) {
                urlResults[3] = response.url;
                statusResults[3] = response.status;
                headerResults[3] = response.headers.get("Source");   
                finishThisTest();
            }, function(error) {
                log(error);
                finishSWTest();
            });
        }
    } catch(e) {
        log("Got exception: " + e);
        finishSWTest();
    }
}

test();

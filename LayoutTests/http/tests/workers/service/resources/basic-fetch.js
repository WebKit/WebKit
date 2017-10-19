function done()
{
    finishSWTest();
}

async function test()
{
    try {
        await navigator.serviceWorker.register("resources/basic-fetch-worker.js", { });

        var response = await fetch("test1");
        console.log("test1 status code: " + response.status);
        console.log("test1 status text: " + response.statusText);

        var response = await fetch("test2");
        console.log("test2 status code: " + response.status);
        console.log("test2 status text: " + response.statusText);

        try {
            response = await fetch("test3");
            console.log("test3 fetch succeeded unexpectedly");
            console.log("test3 status code: " + response.status);
            console.log("test3 status text: " + response.statusText);

        } catch (e) {
            console.log("test3 fetch failed as expected");
        }
    } catch(e) {
        console.log("Got exception: " + e);
    }
    finishSWTest();
}

test();

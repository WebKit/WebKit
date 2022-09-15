importScripts("resource-utilities.js");

const functions = {
    triggerListener() {
        dispatchEvent(new Event("custom"));
    },
    triggerTimeout() {
        setTimeout(function timeout() {
            console.log("timeout fired");
        });
    },
    triggerInterval() {
        let intervalId = setInterval(function interval() {
            console.log("interval fired");
            clearInterval(intervalId);
        });
    },
    triggerXHRRequest() {
        loadResourceXHR("dataXHR.json");
    },
    triggerFetchRequest() {
        loadResourceFetch("dataFetch.json");
    },
    triggerDOMRequest() {
        loadResourceDOM("dataDOM.json");
    },
};

addEventListener("custom", function handler(event) {
    console.log("listener fired");
});

addEventListener("message", function handleMessage(event) {
    functions[event.data]();
});

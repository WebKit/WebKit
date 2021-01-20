resource_entry_type_test({
    name: "xmlhttprequest (asynchronous)",
    url: uniqueDataURL("xhr-async"),
    initiatorType: "xmlhttprequest",
    generateResource(url) {
        let xhr = new XMLHttpRequest;
        xhr.open("GET", url, true);
        xhr.send();
    }
});

resource_entry_type_test({
    name: "xmlhttprequest (synchronous)",
    url: uniqueDataURL("xhr-sync"),
    initiatorType: "xmlhttprequest",
    generateResource(url) {
        let xhr = new XMLHttpRequest;
        xhr.open("GET", url, true);
        xhr.send();
    }
});

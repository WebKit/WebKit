resource_entry_type_test({
    name: "fetch",
    url: uniqueDataURL("fetch"),
    initiatorType: "fetch",
    generateResource(url) {
        fetch(url);
    }
});

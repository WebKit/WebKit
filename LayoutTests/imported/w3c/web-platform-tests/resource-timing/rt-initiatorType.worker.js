importScripts("/resources/testharness.js");
importScripts("resources/rt-utilities.js");

(function() {
    if (!testNecessaryPerformanceFeatures()) {
        done();
        return;
    }

    importScripts("rt-initiatorType-fetch.js");
    importScripts("rt-initiatorType-xmlhttprequest.js");

    resource_entry_type_test({
        name: "other - importScripts",
        url: uniqueScriptURL("importScripts"),
        initiatorType: "other",
        generateResource(url) {
            importScripts(url);
        }
    });

    done();
})();

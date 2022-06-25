if (self.importScripts)
    importScripts("../../resources/js-test-pre.js");

self.jsTestIsAsync = true;

if (self.window)
    description("Tests performance.measure startName/endName must exist.");

shouldNotThrow(`performance.mark("existing-mark-name")`);

shouldThrow(`performance.measure("measure-name", "x")`);
shouldThrow(`performance.measure("measure-name", "existing-mark-name", "x")`);

shouldNotThrow(`performance.mark("x")`);

shouldNotThrow(`performance.measure("measure-name", "x")`);
shouldNotThrow(`performance.measure("measure-name", "existing-mark-name", "x")`);

if (self.importScripts)
    finishJSTest();

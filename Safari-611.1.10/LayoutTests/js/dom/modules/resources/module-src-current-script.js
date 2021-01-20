debug("Module execution is confined in the module environment.");
shouldBe("document.currentScript", "null");
finishJSTest();

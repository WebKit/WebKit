if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Check to make sure IndexedDB in workers can be disabled at runtime");

shouldBeUndefined("self.indexedDB");

finishJSTest();

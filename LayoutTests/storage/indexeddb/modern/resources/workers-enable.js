if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Check to make sure we can enable IndexedDB in workers via a runtime setting");

shouldBeDefined("self.indexedDB");
shouldBeNonNull("self.indexedDB");
shouldBeTrue("self.indexedDB instanceof IDBFactory");

finishJSTest();

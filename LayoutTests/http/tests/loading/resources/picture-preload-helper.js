function loadFromSource(i) {
    shouldBeFalse("internals.isPreloaded('resources/preload-test.jpg?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image1.png?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image2.png?" + i + "');");
    shouldBeTrue("internals.isPreloaded('resources/base-image3.png?" + i + "');");
}
function loadFromSource2x(i) {
    shouldBeFalse("internals.isPreloaded('resources/preload-test.jpg?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image1.png?" + i + "');");
    shouldBeTrue("internals.isPreloaded('resources/base-image2.png?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image3.png?" + i + "');");
}
function loadFromImg(i) {
    shouldBeTrue("internals.isPreloaded('resources/preload-test.jpg?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image1.png?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image2.png?" + i + "');");
    shouldBeFalse("internals.isPreloaded('resources/base-image3.png?" + i + "');");
}

description("requestFileSystemSync PERSISTENT test.");

var fileSystem = requestFileSystemSync(PERSISTENT, 100);

debug("Successfully obtained PERSISTENT FileSystem:" + fileSystem.name);
shouldBeTrue("fileSystem.name.length > 0");
shouldBe("fileSystem.root.fullPath", '"/"');
finishJSTest();
var successfullyParsed = true;

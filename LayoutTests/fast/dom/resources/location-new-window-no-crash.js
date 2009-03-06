description("Tests that manipulating location properties in a just-created window object does not crash.");

var testWindow = open("data:text/plain,a");

// Note that the window does not navigate to the new URL right away, and that is a crucial element
// of the test. We're checking behavior when the object was just created and is not yet at its new
// location.

shouldBe("testWindow.location.toString()", "'/'"); // Firefox returns about:blank
shouldBe("testWindow.location.href", "'/'"); // Firefox returns about:blank
shouldBe("testWindow.location.protocol", "':'"); // Firefox returns about:
shouldBe("testWindow.location.host", "''"); // Firefox throws an exception
shouldBe("testWindow.location.hostname", "''"); // Firefox throws an exception
shouldBe("testWindow.location.port", "''");
shouldBe("testWindow.location.pathname", "'/'"); // Firefox returns the empty string
shouldBe("testWindow.location.search", "''");
shouldBe("testWindow.location.hash", "''");

shouldBe("testWindow.location.href = 'data:text/plain,b'", "'data:text/plain,b'");
shouldBe("testWindow.location.protocol = 'data'", "'data'"); // Firefox throws an exception
shouldBe("testWindow.location.host = 'c'", "'c'"); // Firefox throws an exception
shouldBe("testWindow.location.hostname = 'd'", "'d'"); // Firefox throws an exception
shouldBe("testWindow.location.port = 'e'", "'e'"); // Firefox throws an exception
shouldBe("testWindow.location.pathname = 'f'", "'f'"); // Firefox throws an exception
shouldBe("testWindow.location.search = 'g'", "'g'");
shouldBe("testWindow.location.hash = 'h'", "'h'");

shouldBe("testWindow.location.assign('data:text/plain,i')", "undefined"); // Firefox returns about:blank
shouldBe("testWindow.location.replace('data:text/plain,j')", "undefined"); // Firefox returns about:blank
shouldBe("testWindow.location.reload()", "undefined"); // Firefox returns about:blank

shouldBe("testWindow.location.toString()", "'/'"); // Firefox returns about:blank
shouldBe("testWindow.location.href", "'/'"); // Firefox returns about:blank
shouldBe("testWindow.location.protocol", "':'"); // Firefox returns about:
shouldBe("testWindow.location.host", "''"); // Firefox throws an exception
shouldBe("testWindow.location.hostname", "''"); // Firefox throws an exception
shouldBe("testWindow.location.port", "''");
shouldBe("testWindow.location.pathname", "'/'"); // Firefox returns the empty string
shouldBe("testWindow.location.search", "''");
shouldBe("testWindow.location.hash", "''");

testWindow.close();

var successfullyParsed = true;

description("This test verifies that XMLHttpRequest objects has correct default value for its attributes.");

var xhr = new XMLHttpRequest();

shouldBeNull("xhr.onabort");
shouldBeNull("xhr.onerror");
shouldBeNull("xhr.onload");
shouldBeNull("xhr.onloadstart");
shouldBeNull("xhr.onprogress");
shouldBeNull("xhr.onreadystatechange");
shouldBe("xhr.readyState", "0");

shouldBeNull("xhr.upload.onabort");
shouldBeNull("xhr.upload.onerror");
shouldBeNull("xhr.upload.onload");
shouldBeNull("xhr.upload.onloadstart");
shouldBeNull("xhr.upload.onprogress");


var testURL = "http://127.0.0.1:8000";

function testCompleted()
{
  var scriptElement = document.createElement("script");
  scriptElement.src = "/resources/js-test-post-async.js";
  document.body.appendChild(scriptElement);
}

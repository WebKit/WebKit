// runs a test and writes a log
function t(collection, elements) {
  if (window.testRunner)
    testRunner.dumpAsText();

  var log = "",
      r = document.getElementById("r"),
      pass = true
  if(collection.length != elements.length) {
    pass = false
    log += "Got " + collection.length + " elements, expected " + elements.length + ". "
  }
  for(var i = 0, max = collection.length > elements.length ? elements.length : collection.length; i < max; i++) {
    if(collection[i] != elements[i]) {
      pass = false
      log += "Got element `" + collection[i].tagName + "` (" + collection[i].namespaceURI + ")"
      log += ", expected element `" + elements[i].tagName + "` (" + elements[i].namespaceURI + "). "
    }
  }
  r.textContent = pass ? "PASS" : "FAIL (" + log + ")"
}

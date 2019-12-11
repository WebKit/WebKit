var truealert = window.alert;  // we overwrite window.alert sometimes

// Walk up the caller chain and try to find a reference to the Inspector's window
function doAttack() {
  var obj = doAttack.caller;
  for (var i = 0; 
       i < 1000 && (obj.arguments.length == 0 || !obj.arguments[0].target);
       i++) {
    obj = obj.caller;
  }
  if (i == 1000) return;
  var win = obj.arguments[0].target.ownerDocument.defaultView;
  xhr(win);
}

function xhr(win) {
  var xhr = new win.XMLHttpRequest();
  var url = prompt("Test failed. To prove it, I'm going " +
                   "to make a cross-domain XMLHttpRequest. Where " +
                   "would you like me to send it?\n\nHint: You can " +
                   "also try a file:// URL.", "http://www.example.com/");
  xhr.open("GET", url, false);
  xhr.send();
  truealert("Result:\n\n" + xhr.responseText);
}

function instructions(params) {
  var str = "<p>This test tries to make a cross-domain XMLHttpRequest to " +
    "check whether JavaScript object wrappers are working (bug 16837, bug 16011).</p>" +
    "<p>View this page from an http:// URL to ensure that it's in a different " +
    "origin from the Inspector.</p>" +
    "<p>Instructions:</p>" +
    "<ol>" + 
    "<li>Right click the box" + 
    "<img id=logo src='../resources/webkit-background.png'" + 
      "style='border: 1px solid black; display: block; margin: 1em;'>" + 
    "<li>Choose \"Inspect Element\" from the context menu";
  if (params.console) {
    str += "<li>Select the Console";
    str += "<li>Type " + params.trigger + " into the console and hit Enter";
  } else {
    str += "<li>" + params.trigger;
  }
  str +=  "<li>If the test failed, a prompt will appear.</ol>";
  document.write(str);
}

function onWindowLoad() {
  var doc = top.document;
  var b = doc.body;

  var x = doc.getElementById('x');
  if (x) {
    b.removeChild(x);
  }

  x = doc.createElement("iframe");
  x.setAttribute('id','x');
  // appendChild triggers load
  b.appendChild(x);
}

window.addEventListener("load", onWindowLoad, false);

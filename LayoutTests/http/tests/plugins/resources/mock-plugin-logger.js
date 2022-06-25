function xWebkitTestNetscapeLog(str) {
  var console = document.getElementById('xWebkitTestNetscapeConsole');
  if (!console) {
    console = document.createElement('div');
    console.id = 'xWebkitTestNetscapeConsole';
    document.body.insertBefore(console, document.body.childNodes[0])
  }
  
  var text = document.createElement('p');
  text.appendChild(document.createTextNode(str));
  console.appendChild(text);
}

function log(msg) {
   var console = document.getElementById("console");
   var li = document.createElement("li");
   li.appendChild(document.createTextNode(msg));
   console.appendChild(li);
}

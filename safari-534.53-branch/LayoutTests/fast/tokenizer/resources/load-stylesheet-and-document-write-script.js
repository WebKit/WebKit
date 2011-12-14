var stylesheet = document.createElement("link");
stylesheet.rel = "stylesheet";
stylesheet.type = "text/css";
stylesheet.href = "does-not-exist.css";
document.lastChild.firstChild.appendChild(stylesheet);

document.write("<script src=resources/empty_script.js></script>");


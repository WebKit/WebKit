debug("async-false");
second_script = document.createElement("script");
second_script.async = false;
second_script.src = "resources/async.js";
second_script.addEventListener("load", finishJSTest, false);
document.getElementsByTagName("head")[0].appendChild(second_script);

description("PASS if it doesn't crash. See Bug 40158.");


var inputForProgress = document.createElement("input");
inputForProgress.setAttribute("style", "-webkit-appearance: progress-bar;");
document.body.appendChild(inputForProgress);

var inputForMeter = document.createElement("input");
inputForMeter.setAttribute("style", "-webkit-appearance: meter;");
document.body.appendChild(inputForMeter);

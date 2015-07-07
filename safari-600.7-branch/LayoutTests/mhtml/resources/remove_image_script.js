
function onLoad() {
    divToRemove = document.getElementById("thirdDiv");
    divToRemove.parentNode.removeChild(divToRemove);
    if (window.testRunner)
      testRunner.notifyDone();
}

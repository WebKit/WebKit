if (started) {
    executed = true;
    if (canPass) {
        document.getElementById("result").innerText = "PASS: Script executed after appendChild()";
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

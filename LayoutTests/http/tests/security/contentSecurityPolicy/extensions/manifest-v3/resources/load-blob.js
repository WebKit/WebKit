var script = document.createElement("script");
script.onload = function () {
    alert("FAIL should not have loaded blob-URL script.");
    testRunner.notifyDone();
}
script.onerror = function () {
    alert("PASS successfully blocked blob-URL script.");
    testRunner.notifyDone();
}
script.src = window.URL.createObjectURL(new Blob(["alert('FAIL executed blob-URL script.');"]));

document.head.appendChild(script);

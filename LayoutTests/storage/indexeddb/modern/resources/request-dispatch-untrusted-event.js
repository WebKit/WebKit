description("This test verifies dispatching untrusted event should not cause crash.");

setDBNameFromPath();

function loadImage()
{
    evalAndLog("image = new Image();");
    image.onerror = (error) => {
        imageError = error;
        openDatabase();
    };
    // Generate an error event.
    image.src = 'data:';
}

function openDatabase()
{
    evalAndLog("openRequest = indexedDB.open(dbname);");
    openRequest.onupgradeneeded = () => { openRequest.dispatchEvent(imageError); };
    // Ensure there is no crash after error event is dispatched.
    openRequest.onerror = () => { setTimeout(finishJSTest, 0); };
}

loadImage();
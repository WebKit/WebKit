if (jsLoaded) {
    log("Button was clicked.");
    // Use a big timeout value to ensure that error messages do not show up.
    setTimeout(function() { if (runningModal) log("Error: This line should not show up!"); }, 3000);
}

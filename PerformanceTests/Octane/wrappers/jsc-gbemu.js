function jscSetUp() {
    setupGameboy();
}

function jscTearDown() {
  decoded_gameboy_rom = null;
}

function jscRun() {
    runGameboy();
}

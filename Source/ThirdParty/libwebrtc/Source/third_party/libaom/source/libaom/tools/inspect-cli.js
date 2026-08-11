/**
 * This tool lets you test if the compiled Javascript decoder is functioning properly. You'll
 * need to download a SpiderMonkey js-shell to run this script.
 * https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central/
 *
 * Example:
 *   js-shell inspect-cli.js video.ivf
 */
load("inspect.js");
var buffer = read(scriptArgs[0], "binary");
var Module = {
  noExitRuntime: true,
  noInitialRun: true,
  preInit: [],
  preRun: [],
  postRun: [function () {
    printErr(`Loaded Javascript Decoder OK`);
  }],
  memoryInitializerPrefixURL: "bin/",
  arguments: ['input.ivf', 'output.raw'],
  on_frame_decoded_json: function (jsonString) {
    let json = JSON.parse("[" + Module.UTF8ToString(jsonString) + "null]");
    json.forEach(frame => {
      if (frame) {
        print(frame.frame);
      }
    });
  }
};
DecoderModule(Module);
Module.FS.writeFile("/tmp/input.ivf", buffer, { encoding: "binary" });
Module._open_file();
Module._set_layers(0xFFFFFFFF); // Set this to zero if you want to benchmark decoding.
while(true) {
  printErr("Decoding Frame ...");
  if (Module._read_frame()) {
    break;
  }
}

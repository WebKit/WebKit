try {
  let passes = 0;
  let count = 1;
  for (let i = 0; i < count; ++i) {
    var worker = new Worker("many-nested-workers.js");
    worker.onmessage = function(e) {
      if (e.data == "Pass") {
        if (++passes == count)
          postMessage("Pass");
      }
    };
    worker.postMessage("start");
  }
} catch (e) {
  postMessage("Fail: " + e);
}

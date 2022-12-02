function delayByFrames(f, num_frames) {
  function recurse(depth) {
    if (depth == 0)
      f();
    else
      requestAnimationFrame(() => recurse(depth-1));
  }
  recurse(num_frames);
}

// Returns a Promise which is resolved with the event object when the event is
// fired.
function getEvent(eventType) {
  return new Promise(resolve => {
    document.body.addEventListener(eventType, e => resolve(e), {once: true});
  });
}

onmessage = () => {
  postMessage(undefined)
}
new Worker('empty-post-message-service-workers-crash2.js');

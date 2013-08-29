var message_id = 1;
onmessage = function(event) {
  doWork();
};

function doWork() {
  postMessage("Message #" + message_id++);
  setTimeout(doWork, 50);
}

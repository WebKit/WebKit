function drawCanvas(imageId, canvasId, canvasWidth, canvasHeight) {
  var image = document.getElementById(imageId);
  var canvas = document.getElementById(canvasId);
  canvas.width = canvasWidth;
  canvas.height = canvasHeight;
  var context = canvas.getContext('2d');
  context.drawImage(image, 0, 0, canvasWidth, canvasHeight);
}

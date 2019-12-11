// Seeks to the given time and then removes the SVG document's class to trigger
// a reftest snapshot. If pauseFlag is true, animations will be paused.
function setTimeAndSnapshot(timeInSeconds, pauseFlag) {
  var svg = document.documentElement;
  if (pauseFlag) {
    svg.pauseAnimations();
  }
  svg.setCurrentTime(timeInSeconds);
  svg.removeAttribute("class");
}

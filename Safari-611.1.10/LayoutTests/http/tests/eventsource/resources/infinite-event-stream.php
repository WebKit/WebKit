<?php
date_default_timezone_set("America/New_York");
header("Content-Type: text/event-stream");
$counter = rand(1, 10); // a random counter
while (1) {
// 1 is always true, so repeat the while loop forever (aka event-loop)
  $curDate = date(DATE_ISO8601);
  echo "event: ping\n",
       'data: {"time": "' . $curDate . '"}', "\n\n";
  // Send a simple message at random intervals.
  $counter--;
  if (!$counter) {
    echo 'data: This is a message at time ' . $curDate, "\n\n";
    $counter = rand(1, 10); // reset random counter
  }
  // flush the output buffer and send echoed messages to the browser
  while (ob_get_level() > 0) {
    ob_end_flush();
  }
  flush();
  // break the loop if the client aborted the connection (closed the page)
  
  if ( connection_aborted() ) break;
  // sleep for 1 second before running the loop again
  
  sleep(1);
}

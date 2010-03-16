<?php
header("Content-Type: text/event-stream");
?>

: a comment (ignored)
this line will be ignored since there is no field and the line itself is not a field
field: an unknown field that will be ignored
:another commment

data : this line will be ignored since there is a space after data

data
data:
data
: dispatch event with two newlines

data: simple

data: spanning
data:multiple
data
data: lines
data

id: 1
data: id is 1

data: id is still 1

id
data: no id

event: open
data: a message event with the name "open"

<?php flush(); echo "da"; flush(); ?>
ta: a message event with the name "message"

<?php echo "data: a line ending with crlf\r\n"; ?>
data: a line with a : (colon)
<?php echo "data: a line ending with cr\r"; ?>

retry: 10000
: reconnection time set to 10 seconds

retry: one thousand
: ignored invalid reconnection time value

retry
: reset to ua default


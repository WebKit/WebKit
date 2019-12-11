#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/plain\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "\xef\xbb\xbfTest for bug 5187: UTF-8 in long text files breaks at some point.\n\n";
for ($count=1; $count<2000; $count++) {
    print "\x65\xcc\x81";
}
sleep 10;
for ($count=1; $count<2000; $count++) {
    print "\x65\xcc\x81";
}
sleep 10;
for ($count=1; $count<2000; $count++) {
    print "\x65\xcc\x81";
}

#!/usr/bin/perl -w

# Flush STDOUT after each print.
select (STDOUT);
$| = 1;

print "Content-Type: text/css; charset=utf-8\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "\xef\xbb\xbf#result {color:green;}\n";
# Dump some spaces to bypass CFNetwork buffering.
for ($count = 1; $count < 4000; $count++) {
    print "   ";
}

# Delay to force the second line of text to be decoded as a separate chunk.
sleep 1;
print "body {}";

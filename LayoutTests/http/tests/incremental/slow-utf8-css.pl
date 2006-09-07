#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: text/css; charset=utf-8\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "\xef\xbb\xbfTest for bug 10753: The beginning of a CSS file is missing.\n\n";
for ($count=1; $count<4000; $count++) { # dump some BOMs to bypass CFNetwork buffering
    print "\xef\xbb\xbf";
}
print "You should see a bug description one line above (i.e., this line shouldn't be the only one).";

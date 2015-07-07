#!/usr/bin/perl -w

print "Content-type: text/html\n";
print "Cache-control: max-age=60000\n";
print "ETag: \"123456789\"\n";
print "\n";

my $random_number = int(rand(1000000000000));
print << "END";

<script>
var randomNumber =  $random_number;
top.postMessage(randomNumber, "*");
</script>

END

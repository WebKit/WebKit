#!/usr/bin/perl -w

use Config;

print "Content-type: text/plain\n\n";

if ($ENV{'REQUEST_METHOD'} eq "POST") {
    if ($ENV{'CONTENT_LENGTH'}) {
        read(STDIN, $postData, $ENV{'CONTENT_LENGTH'}) || die "Could not get post data\n";
    } else {
        $postData = "";
    }

    @list = split(/&/, $ENV{'QUERY_STRING'});
    @values;
    $values{'start'} = 0;
    $values{'length'} = -1;
    $values{'convert_newlines'} = 0;
    foreach $element (@list) {
        ($key, $value) = split(/=/, $element);
        $values{$key} = $value;
    }

    # 'items' parameter would look like:
    #   <items> := <item>(','<item>)*
    #    <item> := <file> | <data>
    #    <file> := "file":<file-path>
    #    <data> := "data":<data-string>
    @items = split(/,/, $values{'items'});
    $expectedData = "";
    $readPos = $values{'start'};
    $remainingLength = $values{'length'};
    foreach $item (@items) {
        my ($type, $data) = split(/:/, $item);
        my $size;
        if ($type eq "data") {
            $data =~ s/%([A-Fa-f0-9]{2})/pack('C', hex($1))/seg;
            $size = length($data);
            if ($readPos > 0) {
                if ($readPos > $size) {
                    $readPos -= $size;
                    next;
                }
                $data = substr($data, $readPos);
            }
            $data = substr($data, 0, $remainingLength) if ($remainingLength > 0);
        } else {
            $path = $data;
            open(FILE, $path) || next;
            $size = -s $path;
            if ($readPos > 0) {
                if ($readPos > $size) {
                    $readPos -= $size;
                    next;
                }
                seek(FILE, $readPos, 0);
            }
            if ($remainingLength > 0) {
                read(FILE, $data, $remainingLength);
            } else {
                local $/ = undef;
                $data = <FILE>;
            }
            close(FILE);
        }
        $readPos -= $size;
        $expectedData .= $data;
        if ($remainingLength > 0) {
            $remainingLength -= length($data);
            last if $remainingLength <= 0;
        }
    }

    if ($values{'convert_newlines'}) {
        $nativeEnding = ($Config{osname} =~ /(MSWin|cygwin)/) ? "\r\n" : "\n";
        $postData =~ s/$nativeEnding/[NL]/g;
    }

    if ($postData eq $expectedData) {
        print "OK";
    } else {
        print "FAILED";
    }
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
}

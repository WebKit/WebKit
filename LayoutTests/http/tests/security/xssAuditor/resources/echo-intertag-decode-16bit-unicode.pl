#!/usr/bin/perl -wT
use strict;
use CGI;
use Encode;

my $cgi = new CGI;

use constant Unicode16BitEscapeSequenceLength => 6; # e.g. %u26C4
my $unicode16BitEscapeSequenceRegEx = qr#%u([0-9A-Za-z]{1,4})#;

sub isUTF16Surrogate($)
{
    my ($number) = @_; 
    return $number >= 0xD800 && $number <= 0xDFFF;
}

# See <http://en.wikipedia.org/wiki/Percent-encoding#Non-standard_implementations>.
sub decodeRunOf16BitUnicodeEscapeSequences($)
{
    my ($string) = @_;
    my @codeUnits = grep { length($_) } split(/$unicode16BitEscapeSequenceRegEx/, $string);
    my $i = 0;
    my $decodedRun = "";
    while ($i < @codeUnits) {
        # FIXME: We fallback to the UTF-8 character if we don't receive a proper high and low surrogate pair.
        #        Instead, we should add error handling to detect high/low surrogate mismatches and sequences
        #        of the form low surrogate then high surrogate.
        my $hexDigitValueOfPossibleHighSurrogate = hex($codeUnits[$i]);
        if (isUTF16Surrogate($hexDigitValueOfPossibleHighSurrogate) && $i + 1 < @codeUnits) {
            my $hexDigitValueOfPossibleLowSurrogate = hex($codeUnits[$i + 1]);
            if (isUTF16Surrogate($hexDigitValueOfPossibleLowSurrogate)) {
                $decodedRun .= decode("UTF-16LE", pack("S2", $hexDigitValueOfPossibleHighSurrogate, $hexDigitValueOfPossibleLowSurrogate));
                $i += 2;
                next;
            }
        }
        $decodedRun .= chr($hexDigitValueOfPossibleHighSurrogate);
        $i += 1;
    }
    return $decodedRun;
}

sub decode16BitUnicodeEscapeSequences
{
    my ($string) = @_;
    my $stringLength = length($string);
    my $searchPosition = 0;
    my $encodedRunPosition = 0;
    my $decodedPosition = 0;
    my $result = "";
    while (($encodedRunPosition = index($string, "%u", $searchPosition)) >= 0) {
        my $encodedRunEndPosition = $encodedRunPosition;
        while ($stringLength - $encodedRunEndPosition >= Unicode16BitEscapeSequenceLength
            && substr($string, $encodedRunEndPosition, Unicode16BitEscapeSequenceLength) =~ /$unicode16BitEscapeSequenceRegEx/) {
            $encodedRunEndPosition += Unicode16BitEscapeSequenceLength;
        }
        $searchPosition = $encodedRunEndPosition;
        if ($encodedRunEndPosition == $encodedRunPosition) {
            ++$searchPosition;
            next;
        }
        $result .= substr($string, $decodedPosition, $encodedRunPosition - $decodedPosition);
        $result .= decodeRunOf16BitUnicodeEscapeSequences(substr($string, $encodedRunPosition, $encodedRunEndPosition - $encodedRunPosition));
        $decodedPosition = $encodedRunEndPosition;
    }
    $result .= substr($string, $decodedPosition);
    return $result;
}

my $charsetToUse = $cgi->param('charset') ? $cgi->param('charset') : "UTF-8";
print "Content-Type: text/html; charset=$charsetToUse\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print decode16BitUnicodeEscapeSequences($cgi->param('q'));
print "</body>\n";
print "</html>\n";

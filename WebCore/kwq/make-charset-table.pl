#!/usr/bin/perl -w

use strict;


my $MAC_SUPPORTED_ONLY = 1;

my $canonical_name;
my $mib_enum;
my @aliases;
my %name_to_mac_encoding;
my %used_mac_encodings;

my $already_wrote_one = 0;

my $invalid_encoding = "kCFStringEncodingInvalidId";

sub emit_prefix
{
    print TABLE "static const CharsetEntry table[] = {\n";
}

sub emit_suffix
{
    print TABLE ",\n    {NULL,\n     -1,\n     $invalid_encoding}\n};\n";
}

sub emit_line
{
    my ($name, $mibNum, $encodingNum) = @_;
    print TABLE ",\n" if ($already_wrote_one);
    print TABLE '    {"' . $name . '",' . "\n";
    print TABLE "     " . $mib_enum . ",\n";
    print TABLE "     " . $encodingNum . "}";
    $already_wrote_one = 1;
}

sub emit_output 
{
    my ($canonical_name, $mib_enum, @aliases) = @_;
    
    my $mac_string_encoding = $invalid_encoding;

    foreach my $name ($canonical_name, @aliases) {
	$name = lc $name;
	if ($name_to_mac_encoding{$name}) {
	    $mac_string_encoding = $name_to_mac_encoding{$name};
	    $used_mac_encodings{$name} = $name;
	}
    }

    unless ($MAC_SUPPORTED_ONLY && $mac_string_encoding eq $invalid_encoding) {
	foreach my $name ($canonical_name, @aliases) {
	    emit_line($name, $mib_enum, $mac_string_encoding);
        }
    }
}


sub process_mac_encodings {
    while (<MAC_ENCODINGS>) {
	chomp;
	if (my ($id, $name) = /([0-9]*):(.*)/) {
	    $name_to_mac_encoding{lc $name} = $id;
	}
    }
    
    # Hack, treat -E and -I same as non-suffix case.
    # Not sure if this does the right thing or not.
    $name_to_mac_encoding{"iso-8859-8-e"} = $name_to_mac_encoding{"iso-8859-8"};
    $name_to_mac_encoding{"iso-8859-8-i"} = $name_to_mac_encoding{"iso-8859-8"};
}

sub process_iana_charsets {
    while (<CHARSETS>) {
	chomp;
	if ((my $new_canonical_name) = /Name: ([^ \t]*).*/) {
	    emit_output $canonical_name, $mib_enum, @aliases if ($canonical_name);
	    
	    $canonical_name = $new_canonical_name;
	    $mib_enum = "";
	    @aliases = ();
	} elsif ((my $new_mib_enum) = /MIBenum: ([^ \t]*).*/) {
	    $mib_enum = $new_mib_enum;
	} elsif ((my $new_alias) = /Alias: ([^ \t]*).*/) {
	    push @aliases, $new_alias unless ($new_alias eq "None");
	}
    }
}

sub emit_unused_mac_encodings {
    foreach my $name (keys %name_to_mac_encoding) {
	if (! $used_mac_encodings{$name}) {
	    emit_line($name, -1, $name_to_mac_encoding{$name});
	}
    }
}

# Program body

open CHARSETS, "<" . $ARGV[0];
open MAC_ENCODINGS, "<" . $ARGV[1];
open TABLE, ">" . $ARGV[2];

emit_prefix;
process_mac_encodings;
process_iana_charsets;
emit_unused_mac_encodings;
emit_line("japanese-autodetect", -1, "0xAFE"); # hard-code japanese autodetect
emit_suffix;

close TABLE;

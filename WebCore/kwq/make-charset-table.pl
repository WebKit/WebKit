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


sub emit_output 
{
    my ($canonical_name, $mib_enum, @aliases) = @_;
    
    # Bug in the August 23 version of IANA's character-sets document
    if ($canonical_name eq "ISO-10646-J-1" && ! $mib_enum) {
	$mib_enum = "1004";
    }

    my $mac_string_encoding = $invalid_encoding;

    foreach my $name ($canonical_name, @aliases) {
	$name =~ tr/A-Z/a-z/;
	if ($name_to_mac_encoding{$name}) {
	    $mac_string_encoding = $name_to_mac_encoding{$name};
	    $used_mac_encodings{$name} = $name;
	}
    }

    unless ($MAC_SUPPORTED_ONLY && $mac_string_encoding eq $invalid_encoding) {
	foreach my $name ($canonical_name, @aliases) {
	    print TABLE ",\n" if ($already_wrote_one);
            print TABLE '    {"' . ${name} . '",' . "\n";
            print TABLE "     " . ${mib_enum} . ",\n";
            print TABLE "     " . ${mac_string_encoding} . "}";
            $already_wrote_one = 1;
        }
    }
}


sub process_mac_encodings {
    while (<MAC_ENCODINGS>) {
	chomp;
	if (my ($id, $name) = /([0-9]*):(.*)/) {
	    $name =~ tr/A-Z/a-z/;
	    $name_to_mac_encoding{$name} = $id;
	}
    }
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
	    print TABLE ",\n" if ($already_wrote_one);
	    print TABLE '    {"' . ${name} . '",' . "\n";
	    print TABLE "     " . -1 . ",\n";
            print TABLE "     " . $name_to_mac_encoding{$name} . "}";
            $already_wrote_one = 1;
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
emit_suffix;

close TABLE;

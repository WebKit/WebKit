#!/usr/bin/perl -w

use strict;


my $MAC_SUPPORTED_ONLY = 1;

my %aliasesFromCharsetsFile;
my %namesWritten;

my $output = "";

my $error = 0;

sub error ($)
{
    print STDERR @_, "\n";
    $error = 1;
}

sub emit_line
{
    my ($name, $encoding, $flags) = @_;
 
    error "$name shows up twice in output" if $namesWritten{$name};
    $namesWritten{$name} = 1;
    
    $output .= "    { \"$name\", kCFStringEncoding$encoding, $flags },\n";
}

sub process_mac_encodings
{
    my ($filename) = @_;
    
    my %seenMacNames;
    my %seenIANANames;
    
    open MAC_ENCODINGS, $filename or die;
    
    while (<MAC_ENCODINGS>) {
        chomp;
        s/\#.*$//;
        s/\s+$//;
	if (my ($MacName, undef, $flags, $IANANames) = /^(.+?)(, (.+))?: (.+)$/) {
            my %aliases;
            
            my $MacNameWithFlags = $MacName;
            if ($flags) {
                $MacNameWithFlags .= ", " . $flags;
            } else {
                $flags = "NoEncodingFlags";
            }
            error "CFString encoding name $MacName is mentioned twice in mac-encodings.txt" if $seenMacNames{$MacNameWithFlags};
            $seenMacNames{$MacNameWithFlags} = 1;

            # Build the aliases list.
            # Also check that no two names are part of the same entry in the charsets file.
	    my @IANANames = split ", ", $IANANames;
            my $firstName = "";
            my $canonicalFirstName = "";
            my $prevName = "";
            for my $name (@IANANames) {
                if ($firstName eq "") {
                    if ($name !~ /^[-A-Za-z0-9_]+$/) {
                        error "$name, in mac-encodings.txt, has illegal characters in it";
                        next;
                    }
                    $firstName = $name;
                } else {
                    if ($name !~ /^[a-z0-9]+$/) {
                        error "$name, in mac-encodings.txt, has illegal characters in it (must be all lowercase alphanumeric)";
                        next;
                    }
                    if ($name le $prevName) {
                        error "$name comes after $prevName in mac-encodings.txt, but everything must be in alphabetical order";
                    }
                    $prevName = $name;
                }
                
                my $canonicalName = lc $name;
                $canonicalName =~ tr/-_//d;
                
                $canonicalFirstName = $canonicalName if $canonicalFirstName eq "";
                
                error "$name is mentioned twice in mac-encodings.txt" if $seenIANANames{$canonicalName};
                $seenIANANames{$canonicalName} = 1;
                
                $aliases{$canonicalName} = 1;
                next if !$aliasesFromCharsetsFile{$canonicalName};
                for my $alias (@{$aliasesFromCharsetsFile{$canonicalName}}) {
                    $aliases{$alias} = 1;
                }
                for my $otherName (@IANANames) {
                    next if $canonicalName eq $otherName;
                    if ($aliasesFromCharsetsFile{$otherName}
                        && $aliasesFromCharsetsFile{$canonicalName} eq $aliasesFromCharsetsFile{$otherName}
                        && $canonicalName le $otherName) {
                        error "mac-encodings.txt lists both $name and $otherName under $MacName, but that aliasing is already specified in character-sets.txt";
                    }
                }
            }
            
            # write out
            emit_line($firstName, $MacName, $flags);
            for my $alias (sort keys %aliases) {
                emit_line($alias, $MacName, $flags) if $alias ne $canonicalFirstName;
            }
	} elsif (/^([a-zA-Z0-9_]+)(, (.+))?$/) {
            my $MacName = $1;
            
            error "CFString encoding name $MacName is mentioned twice in mac-encodings.txt" if $seenMacNames{$MacName};
            $seenMacNames{$MacName} = 1;
        } elsif (/./) {
            error "syntax error in mac-encodings.txt, line $.";
        }
    }
    
    close MAC_ENCODINGS;
}

sub process_iana_charset 
{
    my ($canonical_name, @aliases) = @_;
    
    return if !$canonical_name;
    
    my @names = sort $canonical_name, @aliases;
    
    for my $name (@names) {
        $aliasesFromCharsetsFile{$name} = \@names;
    }
}

sub process_iana_charsets
{
    my ($filename) = @_;
    
    open CHARSETS, $filename or die;
    
    my %seen;
    
    my $canonical_name;
    my @aliases;
    
    my %exceptions = ( isoir91 => 1, isoir92 => 1 );
    
    while (<CHARSETS>) {
        chomp;
	if ((my $new_canonical_name) = /Name: ([^ \t]*).*/) {
            $new_canonical_name = lc $new_canonical_name;
            $new_canonical_name =~ tr/a-z0-9//cd;
            
            error "saw $new_canonical_name twice in character-sets.txt", if $seen{$new_canonical_name};
            $seen{$new_canonical_name} = $new_canonical_name;
            
	    process_iana_charset $canonical_name, @aliases;
	    
	    $canonical_name = $new_canonical_name;
	    @aliases = ();
	} elsif ((my $new_alias) = /Alias: ([^ \t]*).*/) {
            next if $new_alias eq "None";
            
            $new_alias = lc $new_alias;
            $new_alias =~ tr/a-z0-9//cd;
            
            error "saw $new_alias twice in character-sets.txt $seen{$new_alias}, $canonical_name", if $seen{$new_alias} && $seen{$new_alias} ne $canonical_name && !$exceptions{$new_alias};
            push @aliases, $new_alias if !$seen{$new_alias};
            $seen{$new_alias} = $canonical_name;            
	}
    }
    
    process_iana_charset $canonical_name, @aliases;
    
    close CHARSETS;
}

# Program body

process_iana_charsets($ARGV[0]);
process_mac_encodings($ARGV[1]);

exit 1 if $error;

print "static const CharsetEntry table[] = {\n";
print $output;
print "    { 0, kCFStringEncodingInvalidId, NoEncodingFlags }\n};\n";

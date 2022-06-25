#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util qw(detect_encoding);

use Digest::MD5 qw(md5_base64);
use Encode qw(encode decode resolve_alias is_utf8);
use Getopt::Long;
use Pod::Usage;

#############
# Constants #
#############

use constant IGNORE_ENCODINGS => qw(utf8 ascii iso-8859-1);

use constant MAX_STRING_LEN => 25;

# For certain tables, we can't automatically determine their Primary Key.
# So, we specify it here as a string.
use constant SPECIAL_KEYS => {
    # bugs_activity since 4.4 has a unique primary key added
    bugs_activity     => 'bug_id,bug_when,fieldid',
    profile_setting   => 'user_id,setting_name',
    # profiles_activity since 4.4 has a unique primary key added
    profiles_activity => 'userid,profiles_when,fieldid',
    setting_value     => 'name,value',
    # longdescs didn't used to have a PK, before 2.20.
    longdescs         => 'bug_id,bug_when',
    # The 2.16 versions table lacked a PK
    versions          => 'product_id,value',
    # These are all for earlier versions of Bugzilla. On a modern
    # version of Bugzilla, this script will ignore these (thanks to
    # code further down).
    components        => 'program,value',
    products          => 'product',
};

###############
# Subroutines #
###############

# "truncate" is a file operation in perl, so we can't use that name.
sub trunc {
    my ($str, $should_truncate) = @_;
    return $str if !$should_truncate;
    my $truncated = substr($str, 0, MAX_STRING_LEN);
    if (length($truncated) ne length($str)) {
        $truncated .= '...';
    }
    return $truncated;
}

sub is_valid_utf8 {
    my ($str) = @_;
    Encode::_utf8_on($str);
    return is_utf8($str, 1);
}

###############
# Main Script #
###############

my %switch;
GetOptions(\%switch, 'dry-run', 'guess', 'charset=s', 'show-failures',
                     'overrides=s', 'truncate!', 'help|h');

pod2usage({ -verbose => 1 }) if $switch{'help'};

# You have to specify at least one of these switches.
pod2usage({ -verbose => 0 }) if (!$switch{'charset'} && !$switch{'guess'});

if (exists $switch{'charset'}) {
    $switch{'charset'} = resolve_alias($switch{'charset'})
        || die "'$switch{charset}' is not a valid charset.";
}

if ($switch{'guess'}) {
    if (!eval { require Encode::Detect::Detector }) {
        my $root = ROOT_USER;
        print STDERR <<EOT;
Using --guess requires that Encode::Detect be installed. To install
Encode::Detect, run the following command:

  $^X install-module.pl Encode::Detect

EOT
        exit;
    }
}

my %overrides;
if (exists $switch{'overrides'}) {
    my $file = new IO::File($switch{'overrides'}, 'r') 
        || die "$switch{overrides}: $!";
    my @lines = $file->getlines();
    $file->close();
    foreach my $line (@lines) {
        chomp($line);
        my ($digest, $encoding) = split(' ', $line);
        $overrides{$digest} = $encoding;
    }
}

my $dbh = Bugzilla->dbh;

if ($dbh->isa('Bugzilla::DB::Mysql')) {
    # Get the actual current encoding of the DB.
    my $collation_data = $dbh->selectrow_arrayref(
        "SHOW VARIABLES LIKE 'character_set_database'");
    my $db_charset = $collation_data->[1];
    # Set our connection encoding to *that* encoding, so that MySQL
    # correctly accepts our changes.
    $dbh->do("SET NAMES $db_charset");
    # Make the database give us raw bytes.
    $dbh->do('SET character_set_results = NULL')
}

$dbh->begin_work;

foreach my $table ($dbh->bz_table_list_real) {
    my @columns = $dbh->bz_table_columns($table);

    my $pk = SPECIAL_KEYS->{$table};
    if ($pk) {
        # Assure that we're on a version of Bugzilla where those keys
        # actually exist.
        foreach my $column (split ',', $pk) {
            $pk = undef if !$dbh->bz_column_info($table, $column);
        }
    }

    # Figure out the primary key.
    foreach my $column (@columns) {
        my $def = $dbh->bz_column_info($table, $column);
        $pk = $column if $def->{PRIMARYKEY};
    }
    # If there's no PK, it's defined by a UNIQUE index.
    if (!$pk) {
        foreach my $column (@columns) {
            my $index = $dbh->bz_index_info($table, "${table}_${column}_idx");
            if ($index && ref($index) eq 'HASH') {
                $pk = join(',', @{$index->{FIELDS}}) 
                    if $index->{TYPE} eq 'UNIQUE';
            }
        }
    }

    my $should_truncate = exists $switch{'truncate'} ? $switch{'truncate'} : 1;

    foreach my $column (@columns) {
        my $def = $dbh->bz_column_info($table, $column);
        # If this is a text column, it may need work.
        if ($def->{TYPE} =~ /text|char/i) {
            # If there's still no PK, we're upgrading from 2.14 or earlier.
            # We can't reliably determine the PK (or at least, I don't want to
            # maintain code to record what the PK was at all points in history).
            # So instead we just use the field itself.
            $pk = $column if !$pk;

            print "Converting $table.$column...\n";
            my $sth = $dbh->prepare("SELECT $column, $pk FROM $table 
                                      WHERE $column IS NOT NULL
                                            AND $column != ''");

            my @pk_array = map {"$_ = ?"} split(',', $pk);
            my $pk_where = join(' AND ', @pk_array);
            my $update_sth = $dbh->prepare(
                "UPDATE $table SET $column = ? WHERE $pk_where");

            $sth->execute();

            while (my @result = $sth->fetchrow_array) {
                my $data = shift @result;
                # Wide characters cause md5_base64() to die.
                my $digest_data = utf8::is_utf8($data) 
                                  ? Encode::encode_utf8($data) : $data;
                my $digest = md5_base64($digest_data);

                my @primary_keys = reverse split(',', $pk);
                # We copy the array so that we can pop things from it without
                # affecting the original.
                my @pk_data = @result;
                my $pk_line = join (', ',
                    map { "$_ = " . pop @pk_data } @primary_keys);

                my $encoding;
                if ($switch{'guess'}) {
                    $encoding = detect_encoding($data);

                    # We only show failures if they don't appear to be
                    # ASCII.
                    if ($switch{'show-failures'} && !$encoding
                        && !is_valid_utf8($data) && !$overrides{$digest}) 
                    {
                        my $truncated = trunc($data, $should_truncate);
                        print "Row: [$pk_line]\n",
                              "Failed to guess: Key: $digest",
                              " DATA: $truncated\n";
                    }

                    # If we fail a guess, and the data is valid UTF-8,
                    # just assume we failed because it's UTF-8.
                    next if is_valid_utf8($data);
                }

                # If we couldn't detect the charset (or were instructed
                # not to try), we fall back to --charset. If there's no 
                # fallback, we just do nothing.
                if (!$encoding && $switch{'charset'}) {
                    $encoding = $switch{'charset'};
                }

                $encoding = $overrides{$digest} if $overrides{$digest};

                # We only fix it if it's not ASCII or UTF-8 already.
                if ($encoding && !grep($_ eq $encoding, IGNORE_ENCODINGS)) {
                    my $decoded = encode('utf8', decode($encoding, $data));
                    if ($switch{'dry-run'} && $data ne $decoded) {
                        print "Row:  [$pk_line]\n",
                              "From: [" . trunc($data, $should_truncate) . "] Key: $digest\n",
                              "To:   [" . trunc($decoded, $should_truncate) . "]",
                              " Encoding : $encoding\n";
                    }
                    else {
                        $update_sth->execute($decoded, @result);
                    }
                }
            } # while (my @result = $sth->fetchrow_array)
        } # if ($column->{TYPE} =~ /text|char/i)
    } # foreach my $column (@columns)
}

$dbh->commit;

__END__

=head1 NAME

recode.pl - Converts a database from one encoding (or multiple encodings) 
to UTF-8.

=head1 SYNOPSIS

 contrib/recode.pl [--guess [--show-failures]] [--charset=iso-8859-2]
                   [--overrides=file_name] [--[no-]truncate]

  --dry-run        Don't modify the database.

  --charset        Primary charset your data is currently in. This can be
                   optionally omitted if you do --guess.

  --guess          Try to guess the charset of the data.

  --show-failures  If we fail to guess, show where we failed.

  --overrides      Specify a file containing overrides. See --help
                   for more info.

  --[no-]truncate  Truncate from/to string output (default: yes).

  --help           Display detailed help.

 If you aren't sure what to do, try:

   contrib/recode.pl --guess --charset=cp1252

=head1 OPTIONS

=over

=item --dry-run

Don't modify the database, just print out what the conversions will be.

recode.pl will print out a Key for each item. You can use this in the 
overrides file, described below.

=item --guess 

If your database is in multiple different encodings, specify this switch 
and recode.pl will do its best to determine the original charset of the data.
The detection is usually very reliable.

If recode.pl cannot guess the charset, it will leave the data alone, unless
you've specified --charset.

=item --charset=charset-name

If you do not specify --guess, then your database is converted
from this character set into the UTF-8.

If you have specified --guess, recode.pl will use this charset as
a fallback--when it cannot guess the charset of a particular piece
of data, it will guess that the data is in this charset and convert
it from this charset to UTF-8.

charset-name must be a charset that is known to perl's Encode 
module. To see a list of available charsets, do: 

C<perl -MEncode -e 'print join("\n", Encode-E<gt>encodings(":all"))'>

=item --show-failures

If --guess fails to guess a charset, print out the data it failed on.

=item --overrides=file_name

This is a way of specifying certain encodings to override the encodings of 
--guess. The file is a series of lines. The line should start with the Key 
from --dry-run, and then a space, and then the encoding you'd like to use.

=item  --[no-]truncate

This specifies whether the from/to text that is converted should be
truncated or not.  The default is to truncate the text.

=back

#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use warnings;

use File::Basename;
BEGIN { chdir dirname($0) . "/.."; }
use lib qw(. lib);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Bug;

use Term::ReadLine;
use Data::Dumper;
$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Terse = 1;
$Data::Dumper::Indent = 1;
$Data::Dumper::Useqq = 1;
$Data::Dumper::Maxdepth = 1;
$Data::Dumper::Deparse = 0;

my $sysname = get_text('term', {term => 'Bugzilla'});
my $term = new Term::ReadLine "$sysname Console";
read_history($term);
END { write_history($term) }

while ( defined (my $input = $term->readline("$sysname> ")) ) {
    my @res = eval($input);
    if ($@) {
        warn $@;
    }
    else {
        print Dumper(@res);
    }
}
print STDERR "\n";
exit 0;

# d: full dump (normal behavior is limited to depth of 1)
sub d {
    local $Data::Dumper::Maxdepth = 0;
    local $Data::Dumper::Deparse = 1;
    print Dumper(@_);
    return ();
}

# p: print as a single string (normal behavior puts list items on separate lines)
sub p {
    no warnings; # suppress possible undefined var message 
    print(@_, "\n");
    return ();
}

sub filter {
    my $name = shift;
    my $filter = Bugzilla->template->{SERVICE}->{CONTEXT}->{CONFIG}->{FILTERS}->{$name};
    if (scalar @_) {
        return $filter->(@_);
    }
    else {
        return $filter;
    }
}

sub b { get_object('Bugzilla::Bug', @_) }
sub u { get_object('Bugzilla::User', @_) }
sub f { get_object('Bugzilla::Field', @_) }

sub get_object {
    my $class = shift;
    $_ = shift;
    my @results = ();
    
    if (ref $_ eq 'HASH' && keys %$_) {
        @results = @{$class->match($_)};
    }
    elsif (m/^\d+$/) {
        @results = ($class->new($_));
    }
    elsif (m/\w/i && grep {$_ eq 'name'} ($class->_get_db_columns)) {
        @results = @{$class->match({name => $_})};
    }
    else {
        @results = ();
    }
    
    if (wantarray) {
        return @results;
    }
    else {
        return shift @results;
    }
}

sub read_history {
    my ($term) = @_;
    
    if (open HIST, "<$ENV{HOME}/.bugzilla_console_history") {
        foreach (<HIST>) {
            chomp;
            $term->addhistory($_);
        }
        close HIST;
    }
}

sub write_history {
    my ($term) = @_;

    if ($term->can('GetHistory') && open HIST, ">$ENV{HOME}/.bugzilla_console_history") {
        my %seen_hist = ();
        my @hist = ();
        foreach my $line (reverse $term->GetHistory()) {
            next unless $line =~ m/\S/;
            next if $seen_hist{$line};
            $seen_hist{$line} = 1;
            push @hist, $line;
            last if (scalar @hist > 500);
        }
        foreach (reverse @hist) {
            print HIST $_, "\n";
        }
        close HIST;
    }
}

__END__

=head1 NAME

B<console.pl> - command-line interface to Bugzilla API

=head1 SYNOPSIS

$ B<contrib/console.pl>

Bugzilla> B<b(5)-E<gt>short_desc>

=over 8

"Misplaced Widget"

=back

Bugzilla> B<$f = f "cf_subsystem"; scalar @{$f-E<gt>legal_values}>

=over 8

177

=back

Bugzilla> B<p filter html_light, "1 E<lt> 2 E<lt>bE<gt>3E<lt>/bE<gt>">

=over 8

1 &lt; 2 E<lt>bE<gt>3E<lt>/bE<gt>

=back

Bugzilla> B<$u = u 5; $u-E<gt>groups; d $u>

=head1 DESCRIPTION

Loads Bugzilla packages and prints expressions with Data::Dumper.
Useful for checking results of Bugzilla API calls without opening
a debug file from a cgi.

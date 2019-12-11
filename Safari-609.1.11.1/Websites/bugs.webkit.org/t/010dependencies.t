# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


##################
#Bugzilla Test 10#
## dependencies ##

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib t);

use Support::Files;
use Test::More qw(no_plan);

my %mods;
my %deps;

use constant MODULE_REGEX => qr/
    (?:(?:^\s*use)
       |
       (?:^require)
    )\s+
    ['"]?
    ([\w:\.\\]+)
/x;
use constant BASE_REGEX => qr/^use (?:base|parent) (?:-norequire, )?qw\(([^\)]+)/;

# Extract all Perl modules.
foreach my $file (@Support::Files::testitems) {
  if ($file =~ /^(.*)\.pm$/) {
    my $module = $1;
    $module =~ s#/#::#g;
    $mods{$module} = $file;
  }
}

foreach my $module (keys %mods) {
    my $reading = 1;
    my @use;

    open(SOURCE, $mods{$module});
    while (my $line = <SOURCE>) {
      last if ($line =~ /^__END__/);
      if ($line =~ /^=cut/) {
        $reading = 1;
        next;
      }
      next unless $reading;
      if ($line =~ /^=(head|over|item|back|pod|begin|end|for)/) {
        $reading = 0;
        next;
      }
      if ($line =~ /^package\s+([^;]);/) {
        $module = $1;
      }
      elsif ($line =~ BASE_REGEX or $line =~ MODULE_REGEX) {
        my $used_string = $1;
        # "use base"/"use parent" can have multiple modules
        my @used_array = split(/\s+/, $used_string);
        foreach my $used (@used_array) {
            next if $used !~ /^Bugzilla/;
            $used =~ s#/#::#g;
            $used =~ s#\.pm$##;
            $used =~ s#\$module#[^:]+#;
            $used =~ s#\${[^}]+}#[^:]+#;
            $used =~ s#[" ]##g;
            push(@use, grep(/^\Q$used\E$/, keys %mods));
        }
      }
    }
    close (SOURCE);

    foreach my $u (@use) {
      if (!grep {$_ eq $u} @{$deps{$module}}) {
        push(@{$deps{$module}}, $u);
      }
    }
}

sub creates_loop {
  my ($module, $used_module) = @_;
  my @list = ($used_module);
  my %seen;
  while (my $next = shift @list) {
    if ($module eq $next) {
      ok(0, "Dependency on $used_module from $module causes loop. --ERROR");
      return;
    }
    if (!$seen{$next}) {
      push(@list, @{$deps{$next}}) if defined $deps{$next};
    }
    $seen{$next} = 1;
  }
  ok(1, "No dependency loop between $module and $used_module");
}


foreach my $module (keys %deps) {
  foreach my $used_module (@{$deps{$module}}) {
    creates_loop($module, $used_module);
  }
}

exit 0;

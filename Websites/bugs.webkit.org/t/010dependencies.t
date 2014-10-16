# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code are the Bugzilla Tests.
#
# Contributor(s): David Miller <justdave@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>


##################
#Bugzilla Test 10#
## dependencies ##

use strict;
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
use constant BASE_REGEX => qr/^use base qw\(([^\)]+)/;

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
        # "use base" can have multiple modules
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

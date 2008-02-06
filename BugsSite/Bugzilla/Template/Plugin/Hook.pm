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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>
#

package Bugzilla::Template::Plugin::Hook;

use strict;

use base qw(Template::Plugin);

sub load {
    my ($class, $context) = @_;
    return $class;
}

sub new {
    my ($class, $context) = @_;
    return bless { _CONTEXT => $context }, $class;
}

sub process {
    my ($self, $hook_name) = @_;

    my $paths = $self->{_CONTEXT}->{LOAD_TEMPLATES}->[0]->paths;
    my $template = $self->{_CONTEXT}->stash->{component}->{name};
    my @hooks = ();

    foreach my $path (@$paths) {
        my @files = glob("$path/hook/$template/$hook_name/*.tmpl");

        # Have to remove the templates path (INCLUDE_PATH) from the
        # file path since the template processor auto-adds it back.
        @files = map($_ =~ /^$path\/(.*)$/ ? $1 : {}, @files);

        # Add found files to the list of hooks, but removing duplicates,
        # which can happen when there are identical hooks or duplicate
        # directories in the INCLUDE_PATH (the latter probably being a TT bug).
        foreach my $file (@files) {
            push(@hooks, $file) unless grep($file eq $_, @hooks);
        }
    }

    my $output;
    foreach my $hook (@hooks) {
        $output .= $self->{_CONTEXT}->process($hook);
    }
    return $output;
}

1;

__END__

=head1 NAME

Bugzilla::Template::Plugin::Hook

=head1 DESCRIPTION

Template Toolkit plugin to process hooks added into templates by extensions.

=head1 SEE ALSO

L<Template::Plugin>,
L<http://bugzilla.mozilla.org/show_bug.cgi?id=229658>

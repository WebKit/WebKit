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
#                 Zach Lipton <zach@zachlipton.com>
#                 Elliotte Martin <everythingsolved.com>
#

package Bugzilla::Template::Plugin::Hook;

use strict;

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(include_languages);
use Bugzilla::Template;
use Bugzilla::Util;
use Bugzilla::Error;
use File::Spec;

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
    my ($self, $hook_name, $template) = @_;
    $template ||= $self->{_CONTEXT}->stash->{component}->{name};

    my @hooks;

    # sanity check:
    if (!$template =~ /[\w\.\/\-_\\]+/) {
        ThrowCodeError('template_invalid', { name => $template});
    }

    # also get extension hook files that live in extensions/:
    # parse out the parts of the template name
    my ($vol, $subpath, $filename) = File::Spec->splitpath($template);
    $subpath = $subpath || '';
    $filename =~ m/(.*)\.(.*)\.tmpl/;
    my $templatename = $1;
    my $type = $2;
    # munge the filename to create the extension hook filename:
    my $extensiontemplate = $subpath.'/'.$templatename.'-'.$hook_name.'.'.$type.'.tmpl';
    my @extensions = glob(bz_locations()->{'extensionsdir'} . "/*");
    my @usedlanguages = include_languages({use_languages => Bugzilla->languages});
    foreach my $extension (@extensions) {
        next if -e "$extension/disabled";
        foreach my $language (@usedlanguages) {
            my $file = $extension.'/template/'.$language.'/'.$extensiontemplate;
            if (-e $file) {
                # tt is stubborn and won't take a template file not in its
                # include path, so we open a filehandle and give it to process()
                # so the hook gets invoked:
                open (my $fh, $file);
                push(@hooks, $fh);
            }
        }
    }

    my $paths = $self->{_CONTEXT}->{LOAD_TEMPLATES}->[0]->paths;
    
    # we keep this too since you can still put hook templates in 
    # template/en/custom/hook
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

=head1 METHODS

=over

=item B<process>

=over

=item B<Description>

Processes hooks added into templates by extensions.

=item B<Params>

=over

=item C<hook_name>

The unique name of the template hook.

=item C<template> (optional)

The path of the calling template.
This is used as a work around to a bug which causes the path to the hook
to be incorrect when the hook is called from inside a block.

Example: If the hook C<lastrow> is added to the template
F<show-multiple.html.tmpl> and it is desired to force the correct template
path, the template hook would be:

 [% Hook.process("lastrow", "bug/show-multiple.html.tmpl") %]

=back

=item B<Returns> 

Output from processing template extension.

=back

=back

=head1 SEE ALSO

L<Template::Plugin>

L<http://www.bugzilla.org/docs/tip/html/customization.html>

L<http://bugzilla.mozilla.org/show_bug.cgi?id=229658>

L<http://bugzilla.mozilla.org/show_bug.cgi?id=298341>

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
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Template::Plugin::Hook;
use strict;
use base qw(Template::Plugin);

use Bugzilla::Constants;
use Bugzilla::Install::Util qw(template_include_path); 
use Bugzilla::Util;
use Bugzilla::Error;

use File::Spec;

sub new {
    my ($class, $context) = @_;
    return bless { _CONTEXT => $context }, $class;
}

sub _context { return $_[0]->{_CONTEXT} }

sub process {
    my ($self, $hook_name, $template) = @_;
    my $context = $self->_context();
    $template ||= $context->stash->{component}->{name};

    # sanity check:
    if (!$template =~ /[\w\.\/\-_\\]+/) {
        ThrowCodeError('template_invalid', { name => $template });
    }

    my (undef, $path, $filename) = File::Spec->splitpath($template);
    $path ||= '';
    $filename =~ m/(.+)\.(.+)\.tmpl$/;
    my $template_name = $1;
    my $type = $2;

    # Hooks are named like this:
    my $extension_template = "$path$template_name-$hook_name.$type.tmpl";

    # Get the hooks out of the cache if they exist. Otherwise, read them
    # from the disk.
    my $cache = Bugzilla->request_cache->{template_plugin_hook_cache} ||= {};
    my $lang = $context->{bz_language} || '';
    $cache->{"${lang}__$extension_template"} 
        ||= $self->_get_hooks($extension_template);

    # process() accepts an arrayref of templates, so we just pass the whole
    # arrayref.
    $context->{bz_in_hook} = 1; # See Bugzilla::Template::Context
    return $context->process($cache->{"${lang}__$extension_template"});
}

sub _get_hooks {
    my ($self, $extension_template) = @_;

    my $template_sets = $self->_template_hook_include_path();
    my @hooks;
    foreach my $dir_set (@$template_sets) {
        foreach my $template_dir (@$dir_set) {
            my $file = "$template_dir/hook/$extension_template";
            if (-e $file) {
                my $template = $self->_context->template($file);
                push(@hooks, $template);
                # Don't run the hook for more than one language.
                last;
            }
        }
    }

    return \@hooks;
}

sub _template_hook_include_path {
    my $self = shift;
    my $cache = Bugzilla->request_cache;
    my $language = $self->_context->{bz_language} || '';
    my $cache_key = "template_plugin_hook_include_path_$language";
    $cache->{$cache_key} ||= template_include_path({
        language => $language,
        hook     => 1,
    });
    return $cache->{$cache_key};
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

L<http://wiki.mozilla.org/Bugzilla:Writing_Extensions>

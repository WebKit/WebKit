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
# Contributor(s): Zach Lipton <zach@zachlipton.com>
#

package Bugzilla::Hook;

use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;

use strict;

sub process {
    my ($name, $args) = @_;
    
    # get a list of all extensions
    my @extensions = glob(bz_locations()->{'extensionsdir'} . "/*");
    
    # check each extension to see if it uses the hook
    # if so, invoke the extension source file:
    foreach my $extension (@extensions) {
        # all of these variables come directly from code or directory names. 
        # If there's malicious data here, we have much bigger issues to 
        # worry about, so we can safely detaint them:
        trick_taint($extension);
        # Skip CVS directories and any hidden files/dirs.
        next if $extension =~ m{/CVS$} || $extension =~ m{/\.[^/]+$};
        next if -e "$extension/disabled";
        if (-e $extension.'/code/'.$name.'.pl') {
            Bugzilla->hook_args($args);
            # Allow extensions to load their own libraries.
            local @INC = ("$extension/lib", @INC);
            do($extension.'/code/'.$name.'.pl');
            ThrowCodeError('extension_invalid', 
                { errstr => $@, name => $name, extension => $extension }) if $@;
            # Flush stored data.
            Bugzilla->hook_args({});
        }
    }
}

sub enabled_plugins {
    my $extdir = bz_locations()->{'extensionsdir'};
    my @extensions = glob("$extdir/*");
    my %enabled;
    foreach my $extension (@extensions) {
        trick_taint($extension);
        my $extname = $extension;
        $extname =~ s{^\Q$extdir\E/}{};
        next if $extname eq 'CVS' || $extname =~ /^\./;
        next if -e "$extension/disabled";
        # Allow extensions to load their own libraries.
        local @INC = ("$extension/lib", @INC);
        $enabled{$extname} = do("$extension/info.pl");
        ThrowCodeError('extension_invalid',
                { errstr => $@, name => 'version',
                  extension => $extension }) if $@;

    }

    return \%enabled;
}

1;

__END__

=head1 NAME

Bugzilla::Hook - Extendable extension hooks for Bugzilla code

=head1 SYNOPSIS

 use Bugzilla::Hook;

 Bugzilla::Hook::process("hookname", { arg => $value, arg2 => $value2 });

=head1 DESCRIPTION

Bugzilla allows extension modules to drop in and add routines at 
arbitrary points in Bugzilla code. These points are referred to as
hooks. When a piece of standard Bugzilla code wants to allow an extension
to perform additional functions, it uses Bugzilla::Hook's L</process>
subroutine to invoke any extension code if installed. 

There is a sample extension in F<extensions/example/> that demonstrates
most of the things described in this document, as well as many of the
hooks available.

=head2 How Hooks Work

When a hook named C<HOOK_NAME> is run, Bugzilla will attempt to invoke any 
source files named F<extensions/*/code/HOOK_NAME.pl>.

So, for example, if your extension is called "testopia", and you
want to have code run during the L</install-update_db> hook, you
would have a file called F<extensions/testopia/code/install-update_db.pl>
that contained perl code to run during that hook.

=head2 Arguments Passed to Hooks

Some L<hooks|/HOOKS> have params that are passed to them.

These params are accessible through L<Bugzilla/hook_args>.
That returns a hashref. Very frequently, if you want your
hook to do anything, you have to modify these variables.

=head2 Versioning Extensions

Every extension must have a file in its root called F<info.pl>.
This file must return a hash when called with C<do>.
The hash must contain a 'version' key with the current version of the
extension. Extension authors can also add any extra infomration to this hash if
required, by adding a new key beginning with x_ which will not be used the
core Bugzilla code. 

=head1 SUBROUTINES

=over

=item C<process>

=over

=item B<Description>

Invoke any code hooks with a matching name from any installed extensions.

See C<customization.xml> in the Bugzilla Guide for more information on
Bugzilla's extension mechanism.

=item B<Params>

=over

=item C<$name> - The name of the hook to invoke.

=item C<$args> - A hashref. The named args to pass to the hook. 
They will be accessible to the hook via L<Bugzilla/hook_args>.

=back

=item B<Returns> (nothing)

=back

=back

=head1 HOOKS

This describes what hooks exist in Bugzilla currently. They are mostly
in alphabetical order, but some related hooks are near each other instead
of being alphabetical.

=head2 bug-end_of_update

This happens at the end of L<Bugzilla::Bug/update>, after all other changes are
made to the database. This generally occurs inside a database transaction.

Params:

=over

=item C<bug> - The changed bug object, with all fields set to their updated
values.

=item C<timestamp> - The timestamp used for all updates in this transaction.

=item C<changes> - The hash of changed fields. 
C<$changes-E<gt>{field} = [old, new]>

=back

=head2 buglist-columns

This happens in buglist.cgi after the standard columns have been defined and
right before the display column determination.  It gives you the opportunity
to add additional display columns.

Params:

=over

=item C<columns> - A hashref, where the keys are unique string identifiers
for the column being defined and the values are hashrefs with the
following fields:

=over

=item C<name> - The name of the column in the database.

=item C<title> - The title of the column as displayed to users.

=back

The definition is structured as:

 $columns->{$id} = { name => $name, title => $title };

=back

=head2 colchange-columns

This happens in F<colchange.cgi> right after the list of possible display
columns have been defined and gives you the opportunity to add additional
display columns to the list of selectable columns.

Params:

=over

=item C<columns> - An arrayref containing an array of column IDs.  Any IDs
added by this hook must have been defined in the the buglist-columns hook.
See L</buglist-columns>.

=back

=head2 enter_bug-entrydefaultvars

This happens right before the template is loaded on enter_bug.cgi.

Params:

=over

=item C<vars> - A hashref. The variables that will be passed into the template.

=back

=head2 flag-end_of_update

This happens at the end of L<Bugzilla::Flag/process>, after all other changes
are made to the database and after emails are sent. It gives you a before/after
snapshot of flags so you can react to specific flag changes.
This generally occurs inside a database transaction.

Note that the interface to this hook is B<UNSTABLE> and it may change in the
future.

Params:

=over

=item C<bug> - The changed bug object.

=item C<timestamp> - The timestamp used for all updates in this transaction.

=item C<old_flags> - The snapshot of flag summaries from before the change.

=item C<new_flags> - The snapshot of flag summaries after the change. Call
C<my ($removed, $added) = diff_arrays(old_flags, new_flags)> to get the list of
changed flags, and search for a specific condition like C<added eq 'review-'>.

=back

=head2 install-before_final_checks

Allows execution of custom code before the final checks are done in 
checksetup.pl.

Params:

=over

=item C<silent>

A flag that indicates whether or not checksetup is running in silent mode.

=back

=head2 install-requirements

Because of the way Bugzilla installation works, there can't be a normal
hook during the time that F<checksetup.pl> checks what modules are
installed. (C<Bugzilla::Hook> needs to have those modules installed--it's
a chicken-and-egg problem.)

So instead of the way hooks normally work, this hook just looks for two 
subroutines (or constants, since all constants are just subroutines) in 
your file, called C<OPTIONAL_MODULES> and C<REQUIRED_MODULES>,
which should return arrayrefs in the same format as C<OPTIONAL_MODULES> and
C<REQUIRED_MODULES> in L<Bugzilla::Install::Requirements>.

These subroutines will be passed an arrayref that contains the current
Bugzilla requirements of the same type, in case you want to modify
Bugzilla's requirements somehow. (Probably the most common would be to
alter a version number or the "feature" element of C<OPTIONAL_MODULES>.)

F<checksetup.pl> will add these requirements to its own.

Please remember--if you put something in C<REQUIRED_MODULES>, then
F<checksetup.pl> B<cannot complete> unless the user has that module
installed! So use C<OPTIONAL_MODULES> whenever you can.

=head2 install-update_db

This happens at the very end of all the tables being updated
during an installation or upgrade. If you need to modify your custom
schema, do it here. No params are passed.

=head2 db_schema-abstract_schema

This allows you to add tables to Bugzilla. Note that we recommend that you 
prefix the names of your tables with some word, so that they don't conflict 
with any future Bugzilla tables.

If you wish to add new I<columns> to existing Bugzilla tables, do that
in L</install-update_db>.

Params:

=over

=item C<schema> - A hashref, in the format of 
L<Bugzilla::DB::Schema/ABSTRACT_SCHEMA>. Add new hash keys to make new table
definitions. F<checksetup.pl> will automatically add these tables to the
database when run.

=back

=head2 product-confirm_delete

Called before displaying the confirmation message when deleting a product.

Params:

=over

=item C<vars> - The template vars hashref.

=back

=head2 webservice

This hook allows you to add your own modules to the WebService. (See
L<Bugzilla::WebService>.)

Params:

=over

=item C<dispatch>

A hashref that you can specify the names of your modules and what Perl
module handles the functions for that module. (This is actually sent to 
L<SOAP::Lite/dispatch_with>. You can see how that's used in F<xmlrpc.cgi>.)

The Perl module name must start with C<extensions::yourextension::lib::>
(replace C<yourextension> with the name of your extension). The C<package>
declaration inside that module must also start with 
C<extensions::yourextension::lib::> in that module's code.

Example:

  $dispatch->{Example} = "extensions::example::lib::Example";

And then you'd have a module F<extensions/example/lib/Example.pm>

It's recommended that all the keys you put in C<dispatch> start with the
name of your extension, so that you don't conflict with the standard Bugzilla
WebService functions (and so that you also don't conflict with other
plugins).

=back

=head2 webservice-error_codes

If your webservice extension throws custom errors, you can set numeric
codes for those errors here.

Extensions should use error codes above 10000, unless they are re-using
an already-existing error code.

Params:

=over

=item C<error_map>

A hash that maps the names of errors (like C<invalid_param>) to numbers.
See L<Bugzilla::WebService::Constants/WS_ERROR_CODE> for an example.

=back

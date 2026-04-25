# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Extension;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Install::Util qw(
    extension_code_files extension_template_directory 
    extension_package_directory extension_web_directory);

use File::Basename;
use File::Spec;

####################
# Subclass Methods #
####################

sub new {
    my ($class, $params) = @_;
    $params ||= {};
    bless $params, $class;
    return $params;
}

#######################################
# Class (Bugzilla::Extension) Methods #
#######################################

sub load {
    my ($class, $extension_file, $config_file) = @_;
    my $package;

    # This is needed during checksetup.pl, because Extension packages can 
    # only be loaded once (they return "1" the second time they're loaded,
    # instead of their name). During checksetup.pl, extensions are loaded
    # once by Bugzilla::Install::Requirements, and then later again via
    # Bugzilla->extensions (because of hooks).
    my $map = Bugzilla->request_cache->{extension_requirement_package_map};

    if ($config_file) {
        if ($map and defined $map->{$config_file}) {
            $package = $map->{$config_file};
        }
        else {
            my $name = require $config_file;
            if ($name =~ /^\d+$/) {
                ThrowCodeError('extension_must_return_name',
                               { extension => $config_file, 
                                 returned  => $name });
            }
            $package = "${class}::$name";
        }

        __do_call($package, 'modify_inc', $config_file);
    }

    if ($map and defined $map->{$extension_file}) {
        $package = $map->{$extension_file};
        $package->modify_inc($extension_file) if !$config_file;
    }
    else {
        my $name = require $extension_file;
        if ($name =~ /^\d+$/) {
            ThrowCodeError('extension_must_return_name', 
                           { extension => $extension_file, returned => $name });
        }
        $package = "${class}::$name";
        $package->modify_inc($extension_file) if !$config_file;
    }

    $class->_validate_package($package, $extension_file);
    return $package;
}

sub _validate_package {
    my ($class, $package, $extension_file) = @_;

    # For extensions from data/extensions/additional, we don't have a file
    # name, so we fake it.
    if (!$extension_file) {
        $extension_file = $package;
        $extension_file =~ s/::/\//g;
        $extension_file .= '.pm';
    }

    if (!eval { $package->NAME }) {
        ThrowCodeError('extension_no_name', 
                       { filename => $extension_file, package => $package });
    }

    if (!$package->isa($class)) {
        ThrowCodeError('extension_must_be_subclass',
                       { filename => $extension_file,
                         package  => $package,
                         class    => $class });
    }
}

sub load_all {
    my $class = shift;
    my ($file_sets, $extra_packages) = extension_code_files();
    my @packages;
    foreach my $file_set (@$file_sets) {
        my $package = $class->load(@$file_set);
        push(@packages, $package);
    }

    # Extensions from data/extensions/additional
    foreach my $package (@$extra_packages) {
        # Don't load an "additional" extension if we already have an extension
        # loaded with that name.
        next if grep($_ eq $package, @packages);
        # Untaint the package name
        $package =~ /([\w:]+)/;
        $package = $1;
        eval("require $package") || die $@;
        $package->_validate_package($package);
        push(@packages, $package);
    }

    return \@packages;
}

# Modifies @INC so that extensions can use modules like
# "use Bugzilla::Extension::Foo::Bar", when Bar.pm is in the lib/
# directory of the extension.
sub modify_inc {
    my ($class, $file) = @_;

    # Note that this package_dir call is necessary to set things up
    # for my_inc, even if we didn't take its return value.
    my $package_dir = __do_call($class, 'package_dir', $file);
    # Don't modify @INC for extensions that are just files in the extensions/
    # directory. We don't want Bugzilla's base lib/CGI.pm being loaded as 
    # Bugzilla::Extension::Foo::CGI or any other confusing thing like that.
    return if $package_dir eq bz_locations->{'extensionsdir'};
    unshift(@INC, sub { __do_call($class, 'my_inc', @_) });
}

# This is what gets put into @INC by modify_inc.
sub my_inc {
    my ($class, undef, $file) = @_;
    
    # This avoids infinite recursion in case anything inside of this function
    # does a "require". (I know for sure that File::Spec->case_tolerant does
    # a "require" on Windows, for example.)
    return if $file !~ /^Bugzilla/;

    my $lib_dir = __do_call($class, 'lib_dir');
    my @class_parts = split('::', $class);
    my ($vol, $dir, $file_name) = File::Spec->splitpath($file);
    my @dir_parts = File::Spec->splitdir($dir);
    # File::Spec::Win32 (any maybe other OSes) add an empty directory at the
    # end of @dir_parts.
    @dir_parts = grep { $_ ne '' } @dir_parts;
    # Validate that this is a sub-package of Bugzilla::Extension::Foo ($class).
    for (my $i = 0; $i < scalar(@class_parts); $i++) {
        return if !@dir_parts;
        if (File::Spec->case_tolerant) {
            return if lc($class_parts[$i]) ne lc($dir_parts[0]);
        }
        else {
            return if $class_parts[$i] ne $dir_parts[0];
        }
        shift(@dir_parts);
    }
    # For Bugzilla::Extension::Foo::Bar, this would look something like
    # extensions/Example/lib/Bar.pm
    my $resolved_path = File::Spec->catfile($lib_dir, @dir_parts, $file_name);
    open(my $fh, '<', $resolved_path);
    return $fh;
}

####################
# Instance Methods #
####################

use constant enabled => 1;

sub lib_dir {
    my $invocant = shift;
    my $package_dir = __do_call($invocant, 'package_dir');
    # For extensions that are just files in the extensions/ directory,
    # use the base lib/ dir as our "lib_dir". Note that Bugzilla never
    # uses lib_dir in this case, though, because modify_inc is prevented
    # from modifying @INC when we're just a file in the extensions/ directory.
    # So this particular code block exists just to make lib_dir return
    # something right in case an extension needs it for some odd reason.
    if ($package_dir eq bz_locations()->{'extensionsdir'}) {
        return bz_locations->{'ext_libpath'};
    }
    return File::Spec->catdir($package_dir, 'lib');
}

sub template_dir { return extension_template_directory(@_); }
sub package_dir  { return extension_package_directory(@_);  }
sub web_dir      { return extension_web_directory(@_);      }

######################
# Helper Subroutines #
######################

# In order to not conflict with extensions' private subroutines, any helpers
# here should start with a double underscore.

# This is for methods that can optionally be overridden in Config.pm.
# It falls back to the local implementation if $class cannot do
# the method. This is necessary because Config.pm is not a subclass of
# Bugzilla::Extension.
sub __do_call {
    my ($class, $method, @args) = @_;
    if ($class->can($method)) {
        return $class->$method(@args);
    }
    my $function_ref;
    { no strict 'refs'; $function_ref = \&{$method}; }
    return $function_ref->($class, @args);
}

1;

__END__

=head1 NAME

Bugzilla::Extension - Base class for Bugzilla Extensions.

=head1 SYNOPSIS

The following would be in F<extensions/Foo/Extension.pm> or 
F<extensions/Foo.pm>:

 package Bugzilla::Extension::Foo
 use strict;
 use parent qw(Bugzilla::Extension);

 our $VERSION = '0.02';
 use constant NAME => 'Foo';

 sub some_hook_name { ... }

 __PACKAGE__->NAME;

Custom templates would go into F<extensions/Foo/template/en/default/>.
L<Template hooks|/Template Hooks> would go into 
F<extensions/Foo/template/en/default/hook/>.

=head1 DESCRIPTION

This is the base class for all Bugzilla extensions.

=head1 WRITING EXTENSIONS

The L</SYNOPSIS> above gives a pretty good overview of what's basically
required to write an extension. This section gives more information
on exactly how extensions work and how you write them. There is also a 
L<wiki page|https://wiki.mozilla.org/Bugzilla:Extension_Notes> with additional HOWTOs, tips and tricks.

=head2 Using F<extensions/create.pl>

There is a script, L<extensions::create>, that will set up the framework
of a new extension for you. To use it, pick a name for your extension
and, in the base bugzilla directory, do:

C<extensions/create.pl NAME>

But replace C<NAME> with the name you picked for your extension. That
will create a new directory in the F<extensions/> directory with the name
of your extension. The directory will contain a full framework for
a new extension, with helpful comments in each file describing things
about them.

=head2 Example Extension

There is a sample extension in F<extensions/Example/> that demonstrates
most of the things described in this document, so if you find the
documentation confusing, try just reading the code instead.

=head2 Where Extension Code Goes

Extension code lives under the F<extensions/> directory in Bugzilla.

There are two ways to write extensions:

=over

=item 1

If your extension will have only code and no templates or other files,
you can create a simple C<.pm> file in the F<extensions/> directory. 

For example, if you wanted to create an extension called "Foo" using this
method, you would put your code into a file called F<extensions/Foo.pm>.

=item 2

If you plan for your extension to have templates and other files, you
can create a whole directory for your extension, and the main extension
code would go into a file called F<Extension.pm> in that directory.

For example, if you wanted to create an extension called "Foo" using this
method, you would put your code into a file called
F<extensions/Foo/Extension.pm>.

=back

=head2 The Extension C<NAME>.

The "name" of an extension shows up in several places:

=over

=item 1

The name of the package:

C<package Bugzilla::Extension::Foo;>

=item 2

In a C<NAME> constant that B<must> be defined for every extension:

C<< use constant NAME => 'Foo'; >>

=item 3

At the very end of the file:

C<< __PACKAGE__->NAME; >>

You'll notice that though most Perl packages end with C<1;>, Bugzilla
Extensions must B<always> end with C<< __PACKAGE__->NAME; >>.

=back

The name must be identical in all of those locations.

=head2 Hooks

In L<Bugzilla::Hook>, there is a L<list of hooks|Bugzilla::Hook/HOOKS>.
These are the various areas of Bugzilla that an extension can "hook" into,
which allow your extension to perform code during that point in Bugzilla's
execution.

If your extension wants to implement a hook, all you have to do is
write a subroutine in your hook package that has the same name as
the hook. The subroutine will be called as a method on your extension,
and it will get the arguments specified in the hook's documentation as
named parameters in a hashref.

For example, here's an implementation of a hook named C<foo_start>
that gets an argument named C<bar>:

 sub foo_start {
     my ($self, $args) = @_;
     my $bar = $args->{bar};
     print "I got $bar!\n";
 }

And that would go into your extension's code file--the file that was
described in the L</Where Extension Code Goes> section above.

During your subroutine, you may want to know what values were passed
as CGI arguments  to the current script, or what arguments were passed to
the current WebService method. You can get that data via 
L<Bugzilla/input_params>.

=head3 Adding New Hooks To Bugzilla

If you need a new hook for your extension and you want that hook to be
added to Bugzilla itself, see our development process at 
L<http://wiki.mozilla.org/Bugzilla:Developers>.

In order for a new hook to be accepted into Bugzilla, it has to work,
it must have documentation in L<Bugzilla::Hook>, and it must have example
code in F<extensions/Example/Extension.pm>.

One question that is often asked about new hooks is, "Is this the most
flexible way to implement this hook?" That is, the more power extension
authors get from a hook, the more likely it is to be accepted into Bugzilla.
Hooks that only hook a very specific part of Bugzilla will not be accepted
if their functionality can be accomplished equally well with a more generic
hook.

=head2 If Your Extension Requires Certain Perl Modules

If there are certain Perl modules that your extension requires in order
to run, there is a way you can tell Bugzilla this, and then L<checksetup>
will make sure that those modules are installed, when you run L<checksetup>.

To do this, you need to specify a constant called C<REQUIRED_MODULES>
in your extension. This constant has the same format as
L<Bugzilla::Install::Requirements/REQUIRED_MODULES>.

If there are optional modules that add additional functionality to your
application, you can specify them in a constant called OPTIONAL_MODULES,
which has the same format as 
L<Bugzilla::Install::Requirements/OPTIONAL_MODULES>.

=head3 If Your Extension Needs Certain Modules In Order To Compile

If your extension needs a particular Perl module in order to
I<compile>, then you have a "chicken and egg" problem--in order to
read C<REQUIRED_MODULES>, we have to compile your extension. In order
to compile your extension, we need to already have the modules in
C<REQUIRED_MODULES>!

To get around this problem, Bugzilla allows you to have an additional
file, besides F<Extension.pm>, called F<Config.pm>, that contains
just C<REQUIRED_MODULES>. If you have a F<Config.pm>, it must also
contain the C<NAME> constant, instead of your main F<Extension.pm>
containing the C<NAME> constant.

The contents of the file would look something like this for an extension
named C<Foo>:

  package Bugzilla::Extension::Foo;
  use strict;
  use constant NAME => 'Foo';
  use constant REQUIRED_MODULES => [
    {
      package => 'Some-Package',
      module  => 'Some::Module',
      version => 0,
    }
  ];
  __PACKAGE__->NAME;

Note that it is I<not> a subclass of C<Bugzilla::Extension>, because
at the time that module requirements are being checked in L<checksetup>,
C<Bugzilla::Extension> cannot be loaded. Also, just like F<Extension.pm>,
it ends with C<< __PACKAGE__->NAME; >>. Note also that it has the 
B<exact same> C<package> name as F<Extension.pm>.

This file may not use any Perl modules other than L<Bugzilla::Constants>,
L<Bugzilla::Install::Util>, L<Bugzilla::Install::Requirements>, and 
modules that ship with Perl itself.

If you want to define both C<REQUIRED_MODULES> and C<OPTIONAL_MODULES>,
they must both be in F<Config.pm> or both in F<Extension.pm>.

Every time your extension is loaded by Bugzilla, F<Config.pm> will be
read and then F<Extension.pm> will be read, so your methods in F<Extension.pm>
will have access to everything in F<Config.pm>. Don't define anything
with an identical name in both files, or Perl may throw a warning that
you are redefining things.

This method of setting C<REQUIRED_MODULES> is of course not available if 
your extension is a single file named C<Foo.pm>.

If any of this is confusing, just look at the code of the Example extension.
It uses this method to specify requirements.

=head2 Libraries

Extensions often want to have their own Perl modules. Your extension
can load any Perl module in its F<lib/> directory. (So, if your extension is 
F<extensions/Foo/>, then your Perl modules go into F<extensions/Foo/lib/>.)

However, the C<package> name of your libraries will not work quite
like normal Perl modules do. F<extensions/Foo/lib/Bar.pm> is
loaded as C<Bugzilla::Extension::Foo::Bar>. Or, to say it another way,
C<use Bugzilla::Extension::Foo::Bar;> loads F<extensions/Foo/lib/Bar.pm>,
which should have C<package Bugzilla::Extension::Foo::Bar;> as its package
name.

This allows any place in Bugzilla to load your modules, which is important
for some hooks. It even allows other extensions to load your modules, and 
allows you to install your modules into the global Perl install
as F<Bugzilla/Extension/Foo/Bar.pm>, if you'd like, which helps allow CPAN
distribution of Bugzilla extensions.

B<Note:> If you want to C<use> or C<require> a module that's in 
F<extensions/Foo/lib/> at the top level of your F<Extension.pm>,
you must have a F<Config.pm> (see above) with at least the C<NAME>
constant defined in it.

=head2 Templates

Extensions store templates in a C<template> subdirectory of the extension.
(Obviously, this isn't available for extensions that aren't a directory.)

The format of this directory is exactly like the normal layout of Bugzilla's
C<template> directory--in fact, your extension's C<template> directory
becomes part of Bugzilla's template "search path" as described in
L<Bugzilla::Install::Util/template_include_path>.

You can actually include templates in your extension without having any
C<.pm> files in your extension at all, if you want. (That is, it's entirely
valid to have an extension that's just template files and no code files.)

Bugzilla's templates are written in a language called Template Toolkit.
You can find out more about Template Toolkit at L<http://template-toolkit.org>.

There are two ways to extend or modify Bugzilla's templates: you can use
template hooks (described below) or you can override existing templates
entirely (described further down).

=head2 Template Hooks

Templates can be extended using a system of "hooks" that add new UI elements
to a particular area of Bugzilla without modifying the code of the existing
templates. This is the recommended way for extensions to modify the user
interface of Bugzilla.

=head3 Which Templates Can Be Hooked

There is no list of template hooks like there is for standard code hooks.
To find what places in the user interface can be hooked, search for the 
string C<Hook.process> in Bugzilla's templates (in the 
F<template/en/default/> directory). That will also give you the name of 
the hooks--the first argument to C<Hook.process> is the name of the hook.
(A later section in this document explains how to use that name).

For example, if you see C<Hook.process("additional_header")>, that means 
the name of the hook is C<additional_header>.

=head3 Where Template Hooks Go

To extend templates in your extension using template hooks, you put files into
the F<template/en/default/hook> directory of your extension. So, if you had an
extension called "Foo", your template extensions would go into
F<extensions/Foo/template/en/default/hook/>.

(Note that the base F<template/en/default/hook> directory in Bugzilla itself
also works, although you would never use that for an extension that you
intended to distribute.)

The files that go into this directory have a certain name, based on the
name of the template that is being hooked, and the name of the hook.
For example, let's imagine that you have an extension named "Foo",
and you want to use the C<additional_header> hook in 
F<template/en/default/global/header.html.tmpl>. Your code would go into
F<extensions/Foo/template/en/default/hook/global/header-additional_header.html.tmpl>. Any code you put into that file will happen at the point that 
C<Hook.process("additional_header")> is called in 
F<template/en/default/global/header.html.tmpl>.

As you can see, template extension file names follow a pattern. The
pattern looks like:

 <templates>/hook/<template path>/<template name>-<hook name>.<template type>.tmpl

=over

=item <templates>

This is the full path to the template directory, like 
F<extensions/Foo/template/en/default>. This works much like normal templates
do, in the sense that template extensions in C<custom> override template
extensions in C<default> for your extension, templates for different languages
can be supplied, etc. Template extensions are searched for and run in the
order described in L<Bugzilla::Install::Util/template_include_path>.

The difference between normal templates and template hooks is that hooks
will be run for I<every> extension, whereas for normal templates, Bugzilla
just takes the first one it finds and stops searching. So while a template
extension in the C<custom> directory may override the same-named template
extension in the  C<default> directory I<within your Bugzilla extension>,
it will not override the same-named template extension in the C<default> 
directory of another Bugzilla extension.

=item <template path>

This is the part of the path (excluding the filename) that comes after 
F<template/en/default/> in a template's path. So, for 
F<template/en/default/global/header.html.tmpl>, this would simply be
C<global>.

=item <template name>

This is the file name of the template, before the C<.html.tmpl> part.
So, for F<template/en/default/global/header.html.tmpl>, this would be
C<header>.

=item <hook name>

This is the name of the hook--what you saw in C<Hook.process> inside
of the template you want to hook. In our example, this is 
C<additional_header>.

=item <template type>

This is what comes after the template name but before C<.tmpl> in the
template's path. In most cases this is C<html>, but sometimes it's
C<none>, C<txt>, C<js>, or various other formats, indicating what
type of output the template has.

=back

=head3 Adding New Template Hooks to Bugzilla

Adding new template hooks is just like adding code hooks (see 
L</Adding New Hooks To Bugzilla>) except that you don't have to
document them, and including example code is optional.

=head2 Overriding Existing Templates

Sometimes you don't want to extend a template, you just want to replace
it entirely with your extension's template, or you want to add an entirely
new template to Bugzilla for your extension to use.

To replace the F<template/en/default/global/banner.html.tmpl> template
in an extension named "Foo", create a file called 
F<extensions/Foo/template/en/default/global/banner.html.tmpl>. Note that this
is very similar to the path for a template hook, except that it excludes
F<hook/>, and the template is named I<exactly> like the standard Bugzilla
template. 

You can also use this method to add entirely new templates. If you have
an extension named "Foo", and you add a file named
F<extensions/Foo/template/en/default/foo/bar.html.tmpl>, you can load
that in your code using C<< $template->process('foo/bar.html.tmpl') >>.

=head3 A Warning About Extensions That You Want To Distribute

You should never override an existing Bugzilla template in an
extension that you plan to distribute to others, because only one extension
can override any given template, and which extension will "win" that war
if there are multiple extensions installed is totally undefined.

However, adding new templates in an extension that you want to distribute
is fine, though you have to be careful about how you name them, because
any templates with an identical path and name (say, both called
F<global/stuff.html.tmpl>) will conflict. The usual way to work around
this is to put all your custom templates into a template path that's
named after your extension (since the name of your extension has to be
unique anyway). So if your extension was named Foo, your custom templates
would go into F<extensions/Foo/template/en/default/foo/>. The only
time that doesn't work is with the C<page_before_template> extension, in which
case your templates should probably be in a directory like
F<extensions/Foo/template/en/default/page/foo/> so as not to conflict with
other pages that other extensions might add.

=head2 CSS, JavaScript, and Images

If you include CSS, JavaScript, and images in your extension that are
served directly to the user (that is, they're not read by a script and
then printed--they're just linked directly in your HTML), they should go
into the F<web/> subdirectory of your extension. 

So, for example, if you had a CSS file called F<style.css> and your
extension was called F<Foo>, your file would go into 
F<extensions/Foo/web/style.css>.

=head2 Documenting Extensions

Documentation goes in F<extensions/Foo/docs/en/rst/>, if it's in English, or
change "en" to something else if it's not. The user documentation index file
must be called index-user.rst; the admin documentation must be called
index-admin.rst. These will end up in the User Guide and the Administration
Guide respectively. Both documentation types are optional. You can use various
Sphinx constructs such as :toctree: or :include: to include further reST files
if you need more than one page of docs.

When documenting extensions to the Bugzilla API, if your extension provides
them, the index file would be F<extensions/Foo/docs/en/rst/api/v1/index.rst>.
When and if your API has more than one version, increment the version number.
These docs will get included in the WebService API Reference.

=head2 Disabling Your Extension

If you want your extension to be totally ignored by Bugzilla (it will
not be compiled or seen to exist at all), then create a file called
C<disabled> in your extension's directory. (If your extension is just
a file, like F<extensions/Foo.pm>, you cannot use this method to disable
your extension, and will just have to remove it from the directory if you
want to totally disable it.) Note that if you are running under mod_perl,
you may have to restart your web server for this to take effect.

If you want your extension to be compiled and have L<checksetup> check
for its module pre-requisites, but you don't want the module to be used
by Bugzilla, then you should make your extension's L</enabled> method
return C<0> or some false value.

=head1 DISTRIBUTING EXTENSIONS

If you've made an extension and you want to publish it, the first
thing you'll want to do is package up your extension's code and
then put a link to it in the appropriate section of 
L<http://wiki.mozilla.org/Bugzilla:Addons>.

=head2 Distributing on CPAN

If you want a centralized distribution point that makes it easy
for Bugzilla users to install your extension, it is possible to 
distribute your Bugzilla Extension through CPAN.

The details of making a standard CPAN module are too much to
go into here, but a lot of it is covered in L<perlmodlib>
and on L<http://www.cpan.org/> among other places.

When you distribute your extension via CPAN, your F<Extension.pm>
should simply install itself as F<Bugzilla/Extension/Foo.pm>, 
where C<Foo> is the name of your module. You do not need a separate
F<Config.pm> file, because CPAN itself will handle installing
the prerequisites of your module, so Bugzilla doesn't have to
worry about it.

=head3 Templates in extensions distributed on CPAN

If your extension is F</usr/lib/perl5/Bugzilla/Extension/Foo.pm>,
then Bugzilla will look for templates in the directory
F</usr/lib/perl5/Bugzilla/Extension/Foo/template/>.

You can change this behavior by overriding the L</template_dir>
or L</package_dir> methods described lower down in this document.

=head3 Using an extension distributed on CPAN

There is a file named F<data/extensions/additional> in Bugzilla.
This is a plain-text file. Each line is the name of a module,
like C<Bugzilla::Extension::Foo>. In addition to the extensions
in the F<extensions/> directory, each module listed in this file
will be loaded as a Bugzilla Extension whenever Bugzilla loads or
uses extensions.

=head1 GETTING HELP WITH WRITING EXTENSIONS

If you are an extension author and you'd like some assistance from other
extension authors or the Bugzilla development team, you can use the
normal support channels described at L<http://www.bugzilla.org/support/>.

=head1 ADDITIONAL CONSTANTS

In addition to C<NAME>, there are some other constants you might
want to define:

=head2 C<$VERSION>

This should be a string that describes what version of your extension
this is. Something like C<1.0>, C<1.3.4> or a similar string.

There are no particular restrictions on the format of version numbers,
but you should probably keep them to just numbers and periods, in the
interest of other software that parses version numbers.

By default, this will be C<undef> if you don't define it.

=head1 SUBCLASS METHODS

In addition to hooks, there are a few methods that your extension can
define to modify its behavior, if you want:

=head2 Class Methods

These methods are called on your extension's class. (Like
C<< Bugzilla::Extension::Foo->some_method >>).

=head3 C<new>

Once every request, this method is called on your extension in order
to create an "instance" of it. (Extensions are treated like objects--they
are instantiated once per request in Bugzilla, and then methods are
called on the object.)

=head2 Instance Methods

These are called on an instantiated Extension object.

=head3 C<enabled>

This should return C<1> if this extension's hook code should be run
by Bugzilla, and C<0> otherwise.

=head3 C<package_dir>

This returns the directory that your extension is located in. 

If this is an extension that was installed via CPAN, the directory will 
be the path to F<Bugzilla/Extension/Foo/>, if C<Foo.pm> is the name of your
extension.

If you want to override this method, and you have a F<Config.pm>, you must
override this method in F<Config.pm>.

=head3 C<template_dir>

The directory that your package's templates are in.

This defaults to the C<template> subdirectory of the L</package_dir>.

If you want to override this method, and you have a F<Config.pm>, you must
override this method in F<Config.pm>.

=head3 C<web_dir>

The directory that your package's web related files are in, such as css and javascript.

This defaults to the C<web> subdirectory of the L</package_dir>.

If you want to override this method, and you have a F<Config.pm>, you must
override this method in F<Config.pm>.

=head3 C<lib_dir>

The directory where your extension's libraries are.

This defaults to the C<lib> subdirectory of the L</package_dir>.

If you want to override this method, and you have a F<Config.pm>, you must
override this method in F<Config.pm>.

=head1 BUGZILLA::EXTENSION CLASS METHODS

These are used internally by Bugzilla to load and set up extensions.
If you are an extension author, you don't need to care about these.

=head2 C<load>

Takes two arguments, the path to F<Extension.pm> and the path to F<Config.pm>,
for an extension. Loads the extension's code packages into memory using
C<require>, does some sanity-checking on the extension, and returns the
package name of the loaded extension.

=head2 C<load_all>

Calls L</load> for every enabled extension installed into Bugzilla,
and returns an arrayref of all the package names that were loaded.

=head1 B<Methods in need of POD>

=over

=item modify_inc

=item my_inc

=back

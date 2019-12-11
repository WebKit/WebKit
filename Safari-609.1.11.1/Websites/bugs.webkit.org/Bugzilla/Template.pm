# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


package Bugzilla::Template;

use 5.10.1;
use strict;
use warnings;

use Bugzilla::Constants;
use Bugzilla::WebService::Constants;
use Bugzilla::Hook;
use Bugzilla::Install::Requirements;
use Bugzilla::Install::Util qw(install_string template_include_path 
                               include_languages);
use Bugzilla::Classification;
use Bugzilla::Keyword;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Search;
use Bugzilla::Token;

use Cwd qw(abs_path);
use MIME::Base64;
use Date::Format ();
use Digest::MD5 qw(md5_hex);
use File::Basename qw(basename dirname);
use File::Find;
use File::Path qw(rmtree mkpath);
use File::Spec;
use IO::Dir;
use List::MoreUtils qw(firstidx);
use Scalar::Util qw(blessed);

use parent qw(Template);

use constant FORMAT_TRIPLE => '%19s|%-28s|%-28s';
use constant FORMAT_3_SIZE => [19,28,28];
use constant FORMAT_DOUBLE => '%19s %-55s';
use constant FORMAT_2_SIZE => [19,55];

# Pseudo-constant.
sub SAFE_URL_REGEXP {
    my $safe_protocols = join('|', SAFE_PROTOCOLS);
    return qr/($safe_protocols):[^:\s<>\"][^\s<>\"]+[\w\/]/i;
}

# Convert the constants in the Bugzilla::Constants and Bugzilla::WebService::Constants
# modules into a hash we can pass to the template object for reflection into its "constants" 
# namespace (which is like its "variables" namespace, but for constants). To do so, we
# traverse the arrays of exported and exportable symbols and ignoring the rest
# (which, if Constants.pm exports only constants, as it should, will be nothing else).
sub _load_constants {
    my %constants;
    foreach my $constant (@Bugzilla::Constants::EXPORT,
                          @Bugzilla::Constants::EXPORT_OK)
    {
        if (ref Bugzilla::Constants->$constant) {
            $constants{$constant} = Bugzilla::Constants->$constant;
        }
        else {
            my @list = (Bugzilla::Constants->$constant);
            $constants{$constant} = (scalar(@list) == 1) ? $list[0] : \@list;
        }
    }

    foreach my $constant (@Bugzilla::WebService::Constants::EXPORT, 
                          @Bugzilla::WebService::Constants::EXPORT_OK)
    {
        if (ref Bugzilla::WebService::Constants->$constant) {
            $constants{$constant} = Bugzilla::WebService::Constants->$constant;
        }
        else {
            my @list = (Bugzilla::WebService::Constants->$constant);
            $constants{$constant} = (scalar(@list) == 1) ? $list[0] : \@list;
        }
    }
    return \%constants;
}

# Returns the path to the templates based on the Accept-Language
# settings of the user and of the available languages
# If no Accept-Language is present it uses the defined default
# Templates may also be found in the extensions/ tree
sub _include_path {
    my $lang = shift || '';
    my $cache = Bugzilla->request_cache;
    $cache->{"template_include_path_$lang"} ||= 
        template_include_path({ language => $lang });
    return $cache->{"template_include_path_$lang"};
}

sub get_format {
    my $self = shift;
    my ($template, $format, $ctype) = @_;

    $ctype //= 'html';
    $format //= '';

    # ctype and format can have letters and a hyphen only.
    if ($ctype =~ /[^a-zA-Z\-]/ || $format =~ /[^a-zA-Z\-]/) {
        ThrowUserError('format_not_found', {'format' => $format,
                                            'ctype'  => $ctype,
                                            'invalid' => 1});
    }
    trick_taint($ctype);
    trick_taint($format);

    $template .= ($format ? "-$format" : "");
    $template .= ".$ctype.tmpl";

    # Now check that the template actually exists. We only want to check
    # if the template exists; any other errors (eg parse errors) will
    # end up being detected later.
    eval {
        $self->context->template($template);
    };
    # This parsing may seem fragile, but it's OK:
    # http://lists.template-toolkit.org/pipermail/templates/2003-March/004370.html
    # Even if it is wrong, any sort of error is going to cause a failure
    # eventually, so the only issue would be an incorrect error message
    if ($@ && $@->info =~ /: not found$/) {
        ThrowUserError('format_not_found', {'format' => $format,
                                            'ctype'  => $ctype});
    }

    # Else, just return the info
    return
    {
        'template'    => $template,
        'format'      => $format,
        'extension'   => $ctype,
        'ctype'       => Bugzilla::Constants::contenttypes->{$ctype}
    };
}

# This routine quoteUrls contains inspirations from the HTML::FromText CPAN
# module by Gareth Rees <garethr@cre.canon.co.uk>.  It has been heavily hacked,
# all that is really recognizable from the original is bits of the regular
# expressions.
# This has been rewritten to be faster, mainly by substituting 'as we go'.
# If you want to modify this routine, read the comments carefully

sub quoteUrls {
    my ($text, $bug, $comment, $user) = @_;
    return $text unless $text;
    $user ||= Bugzilla->user;

    # We use /g for speed, but uris can have other things inside them
    # (http://foo/bug#3 for example). Filtering that out filters valid
    # bug refs out, so we have to do replacements.
    # mailto can't contain space or #, so we don't have to bother for that
    # Do this by replacing matches with \x{FDD2}$count\x{FDD3}
    # \x{FDDx} is used because it's unlikely to occur in the text
    # and are reserved unicode characters. We disable warnings for now
    # until we require Perl 5.13.9 or newer.
    no warnings 'utf8';

    # If the comment is already wrapped, we should ignore newlines when
    # looking for matching regexps. Else we should take them into account.
    my $s = ($comment && $comment->already_wrapped) ? qr/\s/ : qr/\h/;

    # However, note that adding the title (for buglinks) can affect things
    # In particular, attachment matches go before bug titles, so that titles
    # with 'attachment 1' don't double match.
    # Dupe checks go afterwards, because that uses ^ and \Z, which won't occur
    # if it was substituted as a bug title (since that always involve leading
    # and trailing text)

    # Because of entities, it's easier (and quicker) to do this before escaping

    my @things;
    my $count = 0;
    my $tmp;

    my @hook_regexes;
    Bugzilla::Hook::process('bug_format_comment',
        { text => \$text, bug => $bug, regexes => \@hook_regexes,
          comment => $comment, user => $user });

    foreach my $re (@hook_regexes) {
        my ($match, $replace) = @$re{qw(match replace)};
        if (ref($replace) eq 'CODE') {
            $text =~ s/$match/($things[$count++] = $replace->({matches => [
                                                               $1, $2, $3, $4,
                                                               $5, $6, $7, $8, 
                                                               $9, $10]}))
                               && ("\x{FDD2}" . ($count-1) . "\x{FDD3}")/egx;
        }
        else {
            $text =~ s/$match/($things[$count++] = $replace) 
                              && ("\x{FDD2}" . ($count-1) . "\x{FDD3}")/egx;
        }
    }

    # Provide tooltips for full bug links (Bug 74355)
    my $urlbase_re = '(' . join('|',
        map { qr/$_/ } grep($_, Bugzilla->params->{'urlbase'}, 
                            Bugzilla->params->{'sslbase'})) . ')';
    $text =~ s~\b(${urlbase_re}\Qshow_bug.cgi?id=\E([0-9]+)(\#c([0-9]+))?)\b
              ~($things[$count++] = get_bug_link($3, $1, { comment_num => $5, user => $user })) &&
               ("\x{FDD2}" . ($count-1) . "\x{FDD3}")
              ~egox;

    # WEBKIT_CHANGES
    # Provides tooltips for short bug links (Bug 169707)
    $text =~ s~\b((https?://)?\Qwebkit.org/b/\E([0-9]+)(\#c([0-9]+))?)\b
              ~($things[$count++] = get_bug_link($3, $1, { comment_num => $5, user => $user })) &&
               ("\x{FDD2}" . ($count-1) . "\x{FDD3}")
              ~egox;

    # non-mailto protocols
    my $safe_protocols = SAFE_URL_REGEXP();
    $text =~ s~\b($safe_protocols)
              ~($tmp = html_quote($1)) &&
               ($things[$count++] = "<a href=\"$tmp\">$tmp</a>") &&
               ("\x{FDD2}" . ($count-1) . "\x{FDD3}")
              ~egox;

    # We have to quote now, otherwise the html itself is escaped
    # THIS MEANS THAT A LITERAL ", <, >, ' MUST BE ESCAPED FOR A MATCH

    $text = html_quote($text);

    # Color quoted text
    $text =~ s~^(&gt;.+)$~<span class="quote">$1</span >~mg;
    $text =~ s~</span >\n<span class="quote">~\n~g;

    # mailto:
    # Use |<nothing> so that $1 is defined regardless
    # &#64; is the encoded '@' character.
    $text =~ s~\b(mailto:|)?([\w\.\-\+\=]+&\#64;[\w\-]+(?:\.[\w\-]+)+)\b
              ~<a href=\"mailto:$2\">$1$2</a>~igx;

    # attachment links
    $text =~ s~\b(attachment$s*\#?$s*([0-9]+)(?:$s+\[details\])?)
              ~($things[$count++] = get_attachment_link($2, $1, $user)) &&
               ("\x{FDD2}" . ($count-1) . "\x{FDD3}")
              ~egmxi;

    # Current bug ID this comment belongs to
    my $current_bugurl = $bug ? ("show_bug.cgi?id=" . $bug->id) : "";

    # This handles bug a, comment b type stuff. Because we're using /g
    # we have to do this in one pattern, and so this is semi-messy.
    # Also, we can't use $bug_re?$comment_re? because that will match the
    # empty string
    my $bug_word = template_var('terms')->{bug};
    my $bug_re = qr/\Q$bug_word\E$s*\#?$s*([0-9]+)/i;
    my $comment_word = template_var('terms')->{comment};
    my $comment_re = qr/(?:\Q$comment_word\E|comment)$s*\#?$s*([0-9]+)/i;
    $text =~ s~\b($bug_re(?:$s*,?$s*$comment_re)?|$comment_re)
              ~ # We have several choices. $1 here is the link, and $2-4 are set
                # depending on which part matched
               (defined($2) ? get_bug_link($2, $1, { comment_num => $3, user => $user }) :
                              "<a href=\"$current_bugurl#c$4\">$1</a>")
              ~egx;

    # Handle a list of bug ids: bugs 1, #2, 3, 4
    # Currently, the only delimiter supported is comma.
    # Concluding "and" and "or" are not supported.
    my $bugs_word = template_var('terms')->{bugs};

    my $bugs_re = qr/\Q$bugs_word\E$s*\#?$s*
                     [0-9]+(?:$s*,$s*\#?$s*[0-9]+)+/ix;

    $text =~ s{($bugs_re)}{
        my $match = $1;
        $match =~ s/((?:#$s*)?([0-9]+))/get_bug_link($2, $1);/eg;
        $match;
    }eg;

    my $comments_word = template_var('terms')->{comments};

    my $comments_re = qr/(?:comments|\Q$comments_word\E)$s*\#?$s*
                         [0-9]+(?:$s*,$s*\#?$s*[0-9]+)+/ix;

    $text =~ s{($comments_re)}{
        my $match = $1;
        $match =~ s|((?:#$s*)?([0-9]+))|<a href="$current_bugurl#c$2">$1</a>|g;
        $match;
    }eg;

    # Old duplicate markers. These don't use $bug_word because they are old
    # and were never customizable.
    $text =~ s~(?<=^\*\*\*\ This\ bug\ has\ been\ marked\ as\ a\ duplicate\ of\ )
               ([0-9]+)
               (?=\ \*\*\*\Z)
              ~get_bug_link($1, $1, { user => $user })
              ~egmx;

    # Now remove the encoding hacks in reverse order
    for (my $i = $#things; $i >= 0; $i--) {
        $text =~ s/\x{FDD2}($i)\x{FDD3}/$things[$i]/eg;
    }

    return $text;
}

# Creates a link to an attachment, including its title.
sub get_attachment_link {
    my ($attachid, $link_text, $user) = @_;
    $user ||= Bugzilla->user;

    my $attachment = new Bugzilla::Attachment({ id => $attachid, cache => 1 });

    if ($attachment) {
        my $title = "";
        my $className = "";
        if ($user->can_see_bug($attachment->bug_id)
            && (!$attachment->isprivate || $user->is_insider))
        {
            $title = $attachment->description;
        }
        if ($attachment->isobsolete) {
            $className = "bz_obsolete";
        }
        # Prevent code injection in the title.
        $title = html_quote(clean_text($title));

        $link_text =~ s/ \[details\]$//;
        my $linkval = "attachment.cgi?id=$attachid";

        # If the attachment is a patch, try to link to the diff rather
        # than the text, by default.
        my $patchlink = "";
        if ($attachment->ispatch and Bugzilla->feature('patch_viewer')) {
            $patchlink = '&amp;action=diff';
        }

        # Whitespace matters here because these links are in <pre> tags.
        return qq|<span class="$className">|
               . qq|<a href="${linkval}${patchlink}" name="attach_${attachid}" title="$title">$link_text</a>|
               . qq| <a href="${linkval}&amp;action=edit" title="$title">[details]</a>|
               . qq|</span>|;
    }
    else {
        return qq{$link_text};
    }
}

# Creates a link to a bug, including its title.
# It takes either two or three parameters:
#  - The bug number
#  - The link text, to place between the <a>..</a>
#  - An optional comment number, for linking to a particular
#    comment in the bug

sub get_bug_link {
    my ($bug, $link_text, $options) = @_;
    $options ||= {};
    $options->{user} ||= Bugzilla->user;

    if (defined $bug && $bug ne '') {
        if (!blessed($bug)) {
            require Bugzilla::Bug;
            $bug = new Bugzilla::Bug({ id => $bug, cache => 1 });
        }
        return $link_text if $bug->{error};
    }

    my $template = Bugzilla->template_inner;
    my $linkified;
    $template->process('bug/link.html.tmpl', 
        { bug => $bug, link_text => $link_text, %$options }, \$linkified);
    return $linkified;
}

# We use this instead of format because format doesn't deal well with
# multi-byte languages.
sub multiline_sprintf {
    my ($format, $args, $sizes) = @_;
    my @parts;
    my @my_sizes = @$sizes; # Copy this so we don't modify the input array.
    foreach my $string (@$args) {
        my $size = shift @my_sizes;
        my @pieces = split("\n", wrap_hard($string, $size));
        push(@parts, \@pieces);
    }

    my $formatted;
    while (1) {
        # Get the first item of each part.
        my @line = map { shift @$_ } @parts;
        # If they're all undef, we're done.
        last if !grep { defined $_ } @line;
        # Make any single undef item into ''
        @line = map { defined $_ ? $_ : '' } @line;
        # And append a formatted line
        $formatted .= sprintf($format, @line);
        # Remove trailing spaces, or they become lots of =20's in
        # quoted-printable emails.
        $formatted =~ s/\s+$//;
        $formatted .= "\n";
    }
    return $formatted;
}

#####################
# Header Generation #
#####################

# Returns the last modification time of a file, as an integer number of
# seconds since the epoch.
sub _mtime { return (stat($_[0]))[9] }

sub mtime_filter {
    my ($file_url, $mtime) = @_;
    # This environment var is set in the .htaccess if we have mod_headers
    # and mod_expires installed, to make sure that JS and CSS with "?"
    # after them will still be cached by clients.
    return $file_url if !$ENV{BZ_CACHE_CONTROL};
    if (!$mtime) {
        my $cgi_path = bz_locations()->{'cgi_path'};
        my $file_path = "$cgi_path/$file_url";
        $mtime = _mtime($file_path);
    }
    return "$file_url?$mtime";
}

# Set up the skin CSS cascade:
#
#  1. standard/global.css
#  2. YUI CSS
#  3. Standard Bugzilla stylesheet set
#  4. Third-party "skin" stylesheet set, per user prefs
#  5. Inline css passed to global/header.html.tmpl
#  6. Custom Bugzilla stylesheet set

sub css_files {
    my ($style_urls, $yui, $yui_css) = @_;

    # global.css goes on every page.
    my @requested_css = ('skins/standard/global.css', @$style_urls);

    my @yui_required_css;
    foreach my $yui_name (@$yui) {
        next if !$yui_css->{$yui_name};
        push(@yui_required_css, "js/yui/assets/skins/sam/$yui_name.css");
    }
    unshift(@requested_css, @yui_required_css);
    
    my @css_sets = map { _css_link_set($_) } @requested_css;
    
    my %by_type = (standard => [], skin => [], custom => []);
    foreach my $set (@css_sets) {
        foreach my $key (keys %$set) {
            push(@{ $by_type{$key} }, $set->{$key});
        }
    }

    # build unified
    $by_type{unified_standard_skin} = _concatenate_css($by_type{standard},
                                                       $by_type{skin});
    $by_type{unified_custom} = _concatenate_css($by_type{custom});

    return \%by_type;
}

sub _css_link_set {
    my ($file_name) = @_;

    my %set = (standard => mtime_filter($file_name));

    # We use (?:^|/) to allow Extensions to use the skins system if they want.
    if ($file_name !~ m{(?:^|/)skins/standard/}) {
        return \%set;
    }

    my $skin = Bugzilla->user->settings->{skin}->{value};
    my $cgi_path = bz_locations()->{'cgi_path'};
    my $skin_file_name = $file_name;
    $skin_file_name =~ s{(?:^|/)skins/standard/}{skins/contrib/$skin/};
    if (my $mtime = _mtime("$cgi_path/$skin_file_name")) {
        $set{skin} = mtime_filter($skin_file_name, $mtime);
    }

    my $custom_file_name = $file_name;
    $custom_file_name =~ s{(?:^|/)skins/standard/}{skins/custom/};
    if (my $custom_mtime = _mtime("$cgi_path/$custom_file_name")) {
        $set{custom} = mtime_filter($custom_file_name, $custom_mtime);
    }

    return \%set;
}

sub _concatenate_css {
    my @sources = map { @$_ } @_;
    return unless @sources;

    my %files =
        map {
            (my $file = $_) =~ s/(^[^\?]+)\?.+/$1/;
            $_ => $file;
        } @sources;

    my $cgi_path   = bz_locations()->{cgi_path};
    my $skins_path = bz_locations()->{assetsdir};

    # build minified files
    my @minified;
    foreach my $source (@sources) {
        next unless -e "$cgi_path/$files{$source}";
        my $file = $skins_path . '/' . md5_hex($source) . '.css';
        if (!-e $file) {
            my $content = read_text("$cgi_path/$files{$source}");

            # minify
            $content =~ s{/\*.*?\*/}{}sg;   # comments
            $content =~ s{(^\s+|\s+$)}{}mg; # leading/trailing whitespace
            $content =~ s{\n}{}g;           # single line

            # rewrite urls
            $content =~ s{url\(([^\)]+)\)}{_css_url_rewrite($source, $1)}eig;

            write_text($file, "/* $files{$source} */\n" . $content . "\n");
        }
        push @minified, $file;
    }

    # concat files
    my $file = $skins_path . '/' . md5_hex(join(' ', @sources)) . '.css';
    if (!-e $file) {
        my $content = '';
        foreach my $source (@minified) {
            $content .= read_text($source);
        }
        write_text($file, $content);
    }

    $file =~ s/^\Q$cgi_path\E\///o;
    return mtime_filter($file);
}

sub _css_url_rewrite {
    my ($source, $url) = @_;
    # rewrite relative urls as the unified stylesheet lives in a different
    # directory from the source
    $url =~ s/(^['"]|['"]$)//g;
    if (substr($url, 0, 1) eq '/' || substr($url, 0, 5) eq 'data:') {
        return 'url(' . $url . ')';
    }
    return 'url(../../' . ($ENV{'PROJECT'} ? '../' : '') . dirname($source) . '/' . $url . ')';
}

sub _concatenate_js {
    return @_ unless CONCATENATE_ASSETS;
    my ($sources) = @_;
    return [] unless $sources;
    $sources = ref($sources) ? $sources : [ $sources ];

    my %files =
        map {
            (my $file = $_) =~ s/(^[^\?]+)\?.+/$1/;
            $_ => $file;
        } @$sources;

    my $cgi_path   = bz_locations()->{cgi_path};
    my $skins_path = bz_locations()->{assetsdir};

    # build minified files
    my @minified;
    foreach my $source (@$sources) {
        next unless -e "$cgi_path/$files{$source}";
        my $file = $skins_path . '/' . md5_hex($source) . '.js';
        if (!-e $file) {
            my $content = read_text("$cgi_path/$files{$source}");

            # minimal minification
            $content =~ s#/\*.*?\*/##sg;    # block comments
            $content =~ s#(^ +| +$)##gm;    # leading/trailing spaces
            $content =~ s#^//.+$##gm;       # single line comments
            $content =~ s#\n{2,}#\n#g;      # blank lines
            $content =~ s#(^\s+|\s+$)##g;   # whitespace at the start/end of file

            write_text($file, ";/* $files{$source} */\n" . $content . "\n");
        }
        push @minified, $file;
    }

    # concat files
    my $file = $skins_path . '/' . md5_hex(join(' ', @$sources)) . '.js';
    if (!-e $file) {
        my $content = '';
        foreach my $source (@minified) {
            $content .= read_text($source);
        }
        write_text($file, $content);
    }

    $file =~ s/^\Q$cgi_path\E\///o;
    return [ $file ];
}

# YUI dependency resolution
sub yui_resolve_deps {
    my ($yui, $yui_deps) = @_;
    
    my @yui_resolved;
    foreach my $yui_name (@$yui) {
        my $deps = $yui_deps->{$yui_name} || [];
        foreach my $dep (reverse @$deps) {
            push(@yui_resolved, $dep) if !grep { $_ eq $dep } @yui_resolved;
        }
        push(@yui_resolved, $yui_name) if !grep { $_ eq $yui_name } @yui_resolved;
    }
    return \@yui_resolved;
}

###############################################################################
# Templatization Code

# The Template Toolkit throws an error if a loop iterates >1000 times.
# We want to raise that limit.
# NOTE: If you change this number, you MUST RE-RUN checksetup.pl!!!
# If you do not re-run checksetup.pl, the change you make will not apply
$Template::Directive::WHILE_MAX = 1000000;

# Use the Toolkit Template's Stash module to add utility pseudo-methods
# to template variables.
use Template::Stash;

# Allow keys to start with an underscore or a dot.
$Template::Stash::PRIVATE = undef;

# Add "contains***" methods to list variables that search for one or more 
# items in a list and return boolean values representing whether or not 
# one/all/any item(s) were found.
$Template::Stash::LIST_OPS->{ contains } =
  sub {
      my ($list, $item) = @_;
      if (ref $item && $item->isa('Bugzilla::Object')) {
          return grep($_->id == $item->id, @$list);
      } else {
          return grep($_ eq $item, @$list);
      }
  };

$Template::Stash::LIST_OPS->{ containsany } =
  sub {
      my ($list, $items) = @_;
      foreach my $item (@$items) { 
          if (ref $item && $item->isa('Bugzilla::Object')) {
              return 1 if grep($_->id == $item->id, @$list);
          } else {
              return 1 if grep($_ eq $item, @$list);
          }
      }
      return 0;
  };

# Clone the array reference to leave the original one unaltered.
$Template::Stash::LIST_OPS->{ clone } =
  sub {
      my $list = shift;
      return [@$list];
  };

# Allow us to sort the list of fields correctly
$Template::Stash::LIST_OPS->{ sort_by_field_name } =
    sub {
        sub field_name {
            if ($_[0] eq 'noop') {
                # Sort --- first
                return '';
            }
            # Otherwise sort by field_desc or description
            return $_[1]{$_[0]} || $_[0];
        }
        my ($list, $field_desc) = @_;
        return [ sort { lc field_name($a, $field_desc) cmp lc field_name($b, $field_desc) } @$list ];
    };

# Allow us to still get the scalar if we use the list operation ".0" on it,
# as we often do for defaults in query.cgi and other places.
$Template::Stash::SCALAR_OPS->{ 0 } = 
  sub {
      return $_[0];
  };

# Add a "truncate" method to the Template Toolkit's "scalar" object
# that truncates a string to a certain length.
$Template::Stash::SCALAR_OPS->{ truncate } = 
  sub {
      my ($string, $length, $ellipsis) = @_;
      return $string if !$length || length($string) <= $length;

      $ellipsis ||= '';
      my $strlen = $length - length($ellipsis);
      my $newstr = substr($string, 0, $strlen) . $ellipsis;
      return $newstr;
  };

# Create the template object that processes templates and specify
# configuration parameters that apply to all templates.

###############################################################################

sub process {
    my $self = shift;
    # All of this current_langs stuff allows template_inner to correctly
    # determine what-language Template object it should instantiate.
    my $current_langs = Bugzilla->request_cache->{template_current_lang} ||= [];
    unshift(@$current_langs, $self->context->{bz_language});
    my $retval = $self->SUPER::process(@_);
    shift @$current_langs;
    return $retval;
}

# Construct the Template object

# Note that all of the failure cases here can't use templateable errors,
# since we won't have a template to use...

sub create {
    my $class = shift;
    my %opts = @_;

    # IMPORTANT - If you make any FILTER changes here, make sure to
    # make them in t/004.template.t also, if required.

    my $config = {
        # Colon-separated list of directories containing templates.
        INCLUDE_PATH => $opts{'include_path'} 
                        || _include_path($opts{'language'}),

        # Remove white-space before template directives (PRE_CHOMP) and at the
        # beginning and end of templates and template blocks (TRIM) for better
        # looking, more compact content.  Use the plus sign at the beginning
        # of directives to maintain white space (i.e. [%+ DIRECTIVE %]).
        PRE_CHOMP => 1,
        TRIM => 1,

        # Bugzilla::Template::Plugin::Hook uses the absolute (in mod_perl)
        # or relative (in mod_cgi) paths of hook files to explicitly compile
        # a specific file. Also, these paths may be absolute at any time
        # if a packager has modified bz_locations() to contain absolute
        # paths.
        ABSOLUTE => 1,
        RELATIVE => $ENV{MOD_PERL} ? 0 : 1,

        COMPILE_DIR => bz_locations()->{'template_cache'},

        # Don't check for a template update until 1 hour has passed since the
        # last check.
        STAT_TTL    => 60 * 60,

        # Initialize templates (f.e. by loading plugins like Hook).
        PRE_PROCESS => ["global/variables.none.tmpl"],

        ENCODING => Bugzilla->params->{'utf8'} ? 'UTF-8' : undef,

        # Functions for processing text within templates in various ways.
        # IMPORTANT!  When adding a filter here that does not override a
        # built-in filter, please also add a stub filter to t/004template.t.
        FILTERS => {

            # Render text in required style.

            inactive => [
                sub {
                    my($context, $isinactive) = @_;
                    return sub {
                        return $isinactive ? '<span class="bz_inactive">'.$_[0].'</span>' : $_[0];
                    }
                }, 1
            ],

            closed => [
                sub {
                    my($context, $isclosed) = @_;
                    return sub {
                        return $isclosed ? '<span class="bz_closed">'.$_[0].'</span>' : $_[0];
                    }
                }, 1
            ],

            obsolete => [
                sub {
                    my($context, $isobsolete) = @_;
                    return sub {
                        return $isobsolete ? '<span class="bz_obsolete">'.$_[0].'</span>' : $_[0];
                    }
                }, 1
            ],

            # Returns the text with backslashes, single/double quotes,
            # and newlines/carriage returns escaped for use in JS strings.
            js => sub {
                my ($var) = @_;
                $var =~ s/([\\\'\"\/])/\\$1/g;
                $var =~ s/\n/\\n/g;
                $var =~ s/\r/\\r/g;
                $var =~ s/\x{2028}/\\u2028/g; # unicode line separator
                $var =~ s/\x{2029}/\\u2029/g; # unicode paragraph separator
                $var =~ s/\@/\\x40/g; # anti-spam for email addresses
                $var =~ s/</\\x3c/g;
                $var =~ s/>/\\x3e/g;
                return $var;
            },
            
            # Converts data to base64
            base64 => sub {
                my ($data) = @_;
                return encode_base64($data);
            },

            # Strips out control characters excepting whitespace
            strip_control_chars => sub {
                my ($data) = @_;
                state $use_utf8 = Bugzilla->params->{'utf8'};
                # Only run for utf8 to avoid issues with other multibyte encodings 
                # that may be reassigning meaning to ascii characters.
                if ($use_utf8) {
                    $data =~ s/(?![\t\r\n])[[:cntrl:]]//g;
                }
                return $data;
            },
            
            # HTML collapses newlines in element attributes to a single space,
            # so form elements which may have whitespace (ie comments) need
            # to be encoded using &#013;
            # See bugs 4928, 22983 and 32000 for more details
            html_linebreak => sub {
                my ($var) = @_;
                $var = html_quote($var);
                $var =~ s/\r\n/\&#013;/g;
                $var =~ s/\n\r/\&#013;/g;
                $var =~ s/\r/\&#013;/g;
                $var =~ s/\n/\&#013;/g;
                return $var;
            },

            xml => \&Bugzilla::Util::xml_quote ,

            # This filter is similar to url_quote but used a \ instead of a %
            # as prefix. In addition it replaces a ' ' by a '_'.
            css_class_quote => \&Bugzilla::Util::css_class_quote ,

            # Removes control characters and trims extra whitespace.
            clean_text => \&Bugzilla::Util::clean_text ,

            quoteUrls => [ sub {
                               my ($context, $bug, $comment, $user) = @_;
                               return sub {
                                   my $text = shift;
                                   return quoteUrls($text, $bug, $comment, $user);
                               };
                           },
                           1
                         ],

            bug_link => [ sub {
                              my ($context, $bug, $options) = @_;
                              return sub {
                                  my $text = shift;
                                  return get_bug_link($bug, $text, $options);
                              };
                          },
                          1
                        ],

            bug_list_link => sub {
                my ($buglist, $options) = @_;
                return join(", ", map(get_bug_link($_, $_, $options), split(/ *, */, $buglist)));
            },

            # In CSV, quotes are doubled, and any value containing a quote or a
            # comma is enclosed in quotes.
            # If a field starts with either "=", "+", "-" or "@", it is preceded
            # by a space to prevent stupid formula execution from Excel & co.
            csv => sub
            {
                my ($var) = @_;
                $var = ' ' . $var if $var =~ /^[+=@-]/;
                # backslash is not special to CSV, but it can be used to confuse some browsers...
                # so we do not allow it to happen. We only do this for logged-in users.
                $var =~ s/\\/\x{FF3C}/g if Bugzilla->user->id;
                $var =~ s/\"/\"\"/g;
                if ($var !~ /^-?(\d+\.)?\d*$/) {
                    $var = "\"$var\"";
                }
                return $var;
            } ,

            # Format a filesize in bytes to a human readable value
            unitconvert => sub
            {
                my ($data) = @_;
                my $retval = "";
                my %units = (
                    'KB' => 1024,
                    'MB' => 1024 * 1024,
                    'GB' => 1024 * 1024 * 1024,
                );

                if ($data < 1024) {
                    return "$data bytes";
                } 
                else {
                    my $u;
                    foreach $u ('GB', 'MB', 'KB') {
                        if ($data >= $units{$u}) {
                            return sprintf("%.2f %s", $data/$units{$u}, $u);
                        }
                    }
                }
            },

            # Format a time for display (more info in Bugzilla::Util)
            time => [ sub {
                          my ($context, $format, $timezone) = @_;
                          return sub {
                              my $time = shift;
                              return format_time($time, $format, $timezone);
                          };
                      },
                      1
                    ],

            html => \&Bugzilla::Util::html_quote,

            html_light => \&Bugzilla::Util::html_light_quote,

            email => \&Bugzilla::Util::email_filter,
            
            mtime => \&mtime_filter,

            # iCalendar contentline filter
            ics => [ sub {
                         my ($context, @args) = @_;
                         return sub {
                             my ($var) = shift;
                             my ($par) = shift @args;
                             my ($output) = "";

                             $var =~ s/[\r\n]/ /g;
                             $var =~ s/([;\\\",])/\\$1/g;

                             if ($par) {
                                 $output = sprintf("%s:%s", $par, $var);
                             } else {
                                 $output = $var;
                             }
                             
                             $output =~ s/(.{75,75})/$1\n /g;

                             return $output;
                         };
                     },
                     1
                     ],

            # Note that using this filter is even more dangerous than
            # using "none," and you should only use it when you're SURE
            # the output won't be displayed directly to a web browser.
            txt => sub {
                my ($var) = @_;
                # Trivial HTML tag remover
                $var =~ s/<[^>]*>//g;
                # And this basically reverses the html filter.
                $var =~ s/\&#64;/@/g;
                $var =~ s/\&lt;/</g;
                $var =~ s/\&gt;/>/g;
                $var =~ s/\&quot;/\"/g;
                $var =~ s/\&amp;/\&/g;
                # Now remove extra whitespace...
                my $collapse_filter = $Template::Filters::FILTERS->{collapse};
                $var = $collapse_filter->($var);
                # And if we're not in the WebService, wrap the message.
                # (Wrapping the message in the WebService is unnecessary
                # and causes awkward things like \n's appearing in error
                # messages in JSON-RPC.)
                unless (i_am_webservice()) {
                    $var = wrap_comment($var, 72);
                }
                $var =~ s/\&nbsp;/ /g;

                return $var;
            },

            # Wrap a displayed comment to the appropriate length
            wrap_comment => [
                sub {
                    my ($context, $cols) = @_;
                    return sub { wrap_comment($_[0], $cols) }
                }, 1],

            # We force filtering of every variable in key security-critical
            # places; we have a none filter for people to use when they 
            # really, really don't want a variable to be changed.
            none => sub { return $_[0]; } ,
        },

        PLUGIN_BASE => 'Bugzilla::Template::Plugin',

        CONSTANTS => _load_constants(),

        # Default variables for all templates
        VARIABLES => {
            # Function for retrieving global parameters.
            'Param' => sub { return Bugzilla->params->{$_[0]}; },

            # Function to create date strings
            'time2str' => \&Date::Format::time2str,

            # Fixed size column formatting for bugmail.
            'format_columns' => sub {
                my $cols = shift;
                my $format = ($cols == 3) ? FORMAT_TRIPLE : FORMAT_DOUBLE;
                my $col_size = ($cols == 3) ? FORMAT_3_SIZE : FORMAT_2_SIZE;
                return multiline_sprintf($format, \@_, $col_size);
            },

            # Generic linear search function
            'lsearch' => sub {
                my ($array, $item) = @_;
                return firstidx { $_ eq $item } @$array;
            },

            # Currently logged in user, if any
            # If an sudo session is in progress, this is the user we're faking
            'user' => sub { return Bugzilla->user; },

            # Currenly active language
            'current_language' => sub { return Bugzilla->current_language; },

            # If an sudo session is in progress, this is the user who
            # started the session.
            'sudoer' => sub { return Bugzilla->sudoer; },

            # Allow templates to access the "correct" URLBase value
            'urlbase' => sub { return Bugzilla::Util::correct_urlbase(); },

            # Allow templates to access docs url with users' preferred language
            # We fall back to English if documentation in the preferred
            # language is not available
            'docs_urlbase' => sub {
                my $docs_urlbase;
                my $lang = Bugzilla->current_language;
                # Translations currently available on readthedocs.org
                my @rtd_translations = ('en', 'fr');

                if ($lang ne 'en' && -f "docs/$lang/html/index.html") {
                    $docs_urlbase = "docs/$lang/html/";
                }
                elsif (-f "docs/en/html/index.html") {
                    $docs_urlbase = "docs/en/html/";
                }
                else {
                    if (!grep { $_ eq $lang } @rtd_translations) {
                        $lang = "en";
                    }

                    my $version = BUGZILLA_VERSION;
                    $version =~ /^(\d+)\.(\d+)/;
                    if ($2 % 2 == 1) {
                        # second number is odd; development version
                        $version = 'latest';
                    }
                    else {
                        $version = "$1.$2";
                    }

                    $docs_urlbase = "https://bugzilla.readthedocs.org/$lang/$version/";
                }

                return $docs_urlbase;
            },

            # Check whether the URL is safe.
            'is_safe_url' => sub {
                my $url = shift;
                return 0 unless $url;

                my $safe_url_regexp = SAFE_URL_REGEXP();
                return 1 if $url =~ /^$safe_url_regexp$/;
                # Pointing to a local file with no colon in its name is fine.
                return 1 if $url =~ /^[^\s<>\":]+[\w\/]$/i;
                # If we come here, then we cannot guarantee it's safe.
                return 0;
            },

            # Allow templates to generate a token themselves.
            'issue_hash_token' => \&Bugzilla::Token::issue_hash_token,

            'get_login_request_token' => sub {
                my $cookie = Bugzilla->cgi->cookie('Bugzilla_login_request_cookie');
                return $cookie ? issue_hash_token(['login_request', $cookie]) : '';
            },

            'get_api_token' => sub {
                return '' unless Bugzilla->user->id;
                my $cache = Bugzilla->request_cache;
                return $cache->{api_token} //= issue_api_token();
            },

            # A way for all templates to get at Field data, cached.
            'bug_fields' => sub {
                my $cache = Bugzilla->request_cache;
                $cache->{template_bug_fields} ||=
                    Bugzilla->fields({ by_name => 1 });
                return $cache->{template_bug_fields};
            },

            # A general purpose cache to store rendered templates for reuse.
            # Make sure to not mix language-specific data.
            'template_cache' => sub {
                my $cache = Bugzilla->request_cache->{template_cache} ||= {};
                $cache->{users} ||= {};
                return $cache;
            },

            'css_files' => \&css_files,
            yui_resolve_deps => \&yui_resolve_deps,
            concatenate_js => \&_concatenate_js,

            # All classifications (sorted by sortkey, name)
            'all_classifications' => sub {
                return [map { $_->name } Bugzilla::Classification->get_all()];
            },

            # Whether or not keywords are enabled, in this Bugzilla.
            'use_keywords' => sub { return Bugzilla::Keyword->any_exist; },

            # All the keywords.
            'all_keywords' => sub {
                return [map { $_->name } Bugzilla::Keyword->get_all()];
            },

            'feature_enabled' => sub { return Bugzilla->feature(@_); },

            # field_descs can be somewhat slow to generate, so we generate
            # it only once per-language no matter how many times
            # $template->process() is called.
            'field_descs' => sub { return template_var('field_descs') },

            # Calling bug/field-help.none.tmpl once per label is very
            # expensive, so we generate it once per-language.
            'help_html' => sub { return template_var('help_html') },

            # This way we don't have to load field-descs.none.tmpl in
            # many templates.
            'display_value' => \&Bugzilla::Util::display_value,

            'install_string' => \&Bugzilla::Install::Util::install_string,

            'report_columns' => \&Bugzilla::Search::REPORT_COLUMNS,

            # These don't work as normal constants.
            DB_MODULE        => \&Bugzilla::Constants::DB_MODULE,
            REQUIRED_MODULES => 
                \&Bugzilla::Install::Requirements::REQUIRED_MODULES,
            OPTIONAL_MODULES => sub {
                my @optional = @{OPTIONAL_MODULES()};
                foreach my $item (@optional) {
                    my @features;
                    foreach my $feat_id (@{ $item->{feature} }) {
                        push(@features, install_string("feature_$feat_id"));
                    }
                    $item->{feature} = \@features;
                }
                return \@optional;
            },
            'default_authorizer' => sub { return Bugzilla::Auth->new() },

            'login_not_email' => sub {
                my $params = Bugzilla->params;
                my $cache = Bugzilla->request_cache;

                return $cache->{login_not_email} //=
                  ($params->{emailsuffix}
                     || ($params->{user_verify_class} =~ /LDAP/ && $params->{LDAPmailattribute})
                     || ($params->{user_verify_class} =~ /RADIUS/ && $params->{RADIUS_email_suffix}))
                  ? 1 : 0;
            },
        },
    };
    # Use a per-process provider to cache compiled templates in memory across
    # requests.
    my $provider_key = join(':', @{ $config->{INCLUDE_PATH} });
    my $shared_providers = Bugzilla->process_cache->{shared_providers} ||= {};
    $shared_providers->{$provider_key} ||= Template::Provider->new($config);
    $config->{LOAD_TEMPLATES} = [ $shared_providers->{$provider_key} ];

    local $Template::Config::CONTEXT = 'Bugzilla::Template::Context';

    Bugzilla::Hook::process('template_before_create', { config => $config });
    my $template = $class->new($config) 
        || die("Template creation failed: " . $class->error());

    # Pass on our current language to any template hooks or inner templates
    # called by this Template object.
    $template->context->{bz_language} = $opts{language} || '';

    return $template;
}

# Used as part of the two subroutines below.
our %_templates_to_precompile;
sub precompile_templates {
    my ($output) = @_;

    # Remove the compiled templates.
    my $cache_dir = bz_locations()->{'template_cache'};
    my $datadir = bz_locations()->{'datadir'};
    if (-e $cache_dir) {
        print install_string('template_removing_dir') . "\n" if $output;

        # This frequently fails if the webserver made the files, because
        # then the webserver owns the directories.
        rmtree($cache_dir);

        # Check that the directory was really removed, and if not, move it
        # into data/deleteme/.
        if (-e $cache_dir) {
            my $deleteme = "$datadir/deleteme";
            
            print STDERR "\n\n",
                install_string('template_removal_failed', 
                               { deleteme => $deleteme, 
                                 template_cache => $cache_dir }), "\n\n";
            mkpath($deleteme);
            my $random = generate_random_password();
            rename($cache_dir, "$deleteme/$random")
              or die "move failed: $!";
        }
    }

    print install_string('template_precompile') if $output;

    # Pre-compile all available languages.
    my $paths = template_include_path({ language => Bugzilla->languages });

    foreach my $dir (@$paths) {
        my $template = Bugzilla::Template->create(include_path => [$dir]);

        %_templates_to_precompile = ();
        # Traverse the template hierarchy.
        find({ wanted => \&_precompile_push, no_chdir => 1 }, $dir);
        # The sort isn't totally necessary, but it makes debugging easier
        # by making the templates always be compiled in the same order.
        foreach my $file (sort keys %_templates_to_precompile) {
            $file =~ s{^\Q$dir\E/}{};
            # Compile the template but throw away the result. This has the side-
            # effect of writing the compiled version to disk.
            $template->context->template($file);
        }

        # Clear out the cached Provider object
        Bugzilla->process_cache->{shared_providers} = undef;
    }

    # Under mod_perl, we look for templates using the absolute path of the
    # template directory, which causes Template Toolkit to look for their 
    # *compiled* versions using the full absolute path under the data/template
    # directory. (Like data/template/var/www/html/bugzilla/.) To avoid
    # re-compiling templates under mod_perl, we symlink to the
    # already-compiled templates. This doesn't work on Windows.
    if (!ON_WINDOWS) {
        # We do these separately in case they're in different locations.
        _do_template_symlink(bz_locations()->{'templatedir'});
        _do_template_symlink(bz_locations()->{'extensionsdir'});
    }

    # If anything created a Template object before now, clear it out.
    delete Bugzilla->request_cache->{template};

    print install_string('done') . "\n" if $output;
}

# Helper for precompile_templates
sub _precompile_push {
    my $name = $File::Find::name;
    return if (-d $name);
    return if ($name =~ /\/CVS\//);
    return if ($name !~ /\.tmpl$/);
    $_templates_to_precompile{$name} = 1;
}

# Helper for precompile_templates
sub _do_template_symlink {
    my $dir_to_symlink = shift;

    my $abs_path = abs_path($dir_to_symlink);

    # If $dir_to_symlink is already an absolute path (as might happen
    # with packagers who set $libpath to an absolute path), then we don't
    # need to do this symlink.
    return if ($abs_path eq $dir_to_symlink);

    my $abs_root  = dirname($abs_path);
    my $dir_name  = basename($abs_path);
    my $cache_dir   = bz_locations()->{'template_cache'};
    my $container = "$cache_dir$abs_root";
    mkpath($container);
    my $target = "$cache_dir/$dir_name";
    # Check if the directory exists, because if there are no extensions,
    # there won't be an "data/template/extensions" directory to link to.
    if (-d $target) {
        # We use abs2rel so that the symlink will look like 
        # "../../../../template" which works, while just 
        # "data/template/template/" doesn't work.
        my $relative_target = File::Spec->abs2rel($target, $container);

        my $link_name = "$container/$dir_name";
        symlink($relative_target, $link_name)
          or warn "Could not make $link_name a symlink to $relative_target: $!";
    }
}

1;

__END__

=head1 NAME

Bugzilla::Template - Wrapper around the Template Toolkit C<Template> object

=head1 SYNOPSIS

  my $template = Bugzilla::Template->create;
  my $format = $template->get_format("foo/bar",
                                     scalar($cgi->param('format')),
                                     scalar($cgi->param('ctype')));

=head1 DESCRIPTION

This is basically a wrapper so that the correct arguments get passed into
the C<Template> constructor.

It should not be used directly by scripts or modules - instead, use
C<Bugzilla-E<gt>instance-E<gt>template> to get an already created module.

=head1 SUBROUTINES

=over

=item C<precompile_templates($output)>

Description: Compiles all of Bugzilla's templates in every language.
             Used mostly by F<checksetup.pl>.

Params:      C<$output> - C<true> if you want the function to print
               out information about what it's doing.

Returns:     nothing

=back

=head1 METHODS

=over

=item C<get_format($file, $format, $ctype)>

 Description: Construct a format object from URL parameters.

 Params:      $file   - Name of the template to display.
              $format - When the template exists under several formats
                        (e.g. table or graph), specify the one to choose.
              $ctype  - Content type, see Bugzilla::Constants::contenttypes.

 Returns:     A format object.

=back

=head1 SEE ALSO

L<Bugzilla>, L<Template>

=head1 B<Methods in need of POD>

=over

=item multiline_sprintf

=item create

=item css_files

=item mtime_filter

=item yui_resolve_deps

=item process

=item get_bug_link

=item quoteUrls

=item get_attachment_link

=item SAFE_URL_REGEXP

=back

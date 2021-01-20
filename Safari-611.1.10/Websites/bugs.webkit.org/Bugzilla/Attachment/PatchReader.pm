# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

package Bugzilla::Attachment::PatchReader;

use 5.10.1;
use strict;
use warnings;

use Config;
use IO::Select;
use IPC::Open3;
use Symbol 'gensym';

use Bugzilla::Error;
use Bugzilla::Attachment;
use Bugzilla::Util;

use constant PERLIO_IS_ENABLED => $Config{useperlio};

sub process_diff {
    my ($attachment, $format) = @_;
    my $dbh = Bugzilla->dbh;
    my $cgi = Bugzilla->cgi;
    my $lc  = Bugzilla->localconfig;
    my $vars = {};

    require PatchReader::Raw;
    my $reader = new PatchReader::Raw;

    if ($format eq 'raw') {
        require PatchReader::DiffPrinter::raw;
        $reader->sends_data_to(new PatchReader::DiffPrinter::raw());
        # Actually print out the patch.
        print $cgi->header(-type => 'text/plain');
        disable_utf8();
        $reader->iterate_string('Attachment ' . $attachment->id, $attachment->data);
    }
    else {
        my @other_patches = ();
        if ($lc->{interdiffbin} && $lc->{diffpath}) {
            # Get the list of attachments that the user can view in this bug.
            my @attachments =
                @{Bugzilla::Attachment->get_attachments_by_bug($attachment->bug)};
            # Extract patches only.
            @attachments = grep {$_->ispatch == 1} @attachments;
            # We want them sorted from newer to older.
            @attachments = sort { $b->id <=> $a->id } @attachments;

            # Ignore the current patch, but select the one right before it
            # chronologically.
            my $select_next_patch = 0;
            foreach my $attach (@attachments) {
                if ($attach->id == $attachment->id) {
                    $select_next_patch = 1;
                }
                else {
                    push(@other_patches, { 'id'       => $attach->id,
                                           'desc'     => $attach->description,
                                           'selected' => $select_next_patch });
                    $select_next_patch = 0;
                }
            }
        }

        $vars->{'bugid'} = $attachment->bug_id;
        $vars->{'attachid'} = $attachment->id;
        $vars->{'description'} = $attachment->description;
        $vars->{'other_patches'} = \@other_patches;

        setup_template_patch_reader($reader, $vars);
        # The patch is going to be displayed in a HTML page and if the utf8
        # param is enabled, we have to encode attachment data as utf8.
        if (Bugzilla->params->{'utf8'}) {
            $attachment->data; # Populate ->{data}
            utf8::decode($attachment->{data});
        }
        $reader->iterate_string('Attachment ' . $attachment->id, $attachment->data);
    }
}

sub process_interdiff {
    my ($old_attachment, $new_attachment, $format) = @_;
    my $cgi = Bugzilla->cgi;
    my $lc  = Bugzilla->localconfig;
    my $vars = {};

    require PatchReader::Raw;

    # Encode attachment data as utf8 if it's going to be displayed in a HTML
    # page using the UTF-8 encoding.
    if ($format ne 'raw' && Bugzilla->params->{'utf8'}) {
        $old_attachment->data; # Populate ->{data}
        utf8::decode($old_attachment->{data});
        $new_attachment->data; # Populate ->{data}
        utf8::decode($new_attachment->{data});
    }

    # Get old patch data.
    my ($old_filename, $old_file_list) = get_unified_diff($old_attachment, $format);
    # Get new patch data.
    my ($new_filename, $new_file_list) = get_unified_diff($new_attachment, $format);

    my $warning = warn_if_interdiff_might_fail($old_file_list, $new_file_list);

    # Send through interdiff, send output directly to template.
    # Must hack path so that interdiff will work.
    local $ENV{'PATH'} = $lc->{diffpath};

    # Open the interdiff pipe, reading from both STDOUT and STDERR
    # To avoid deadlocks, we have to read the entire output from all handles
    my ($stdout, $stderr) = ('', '');
    my ($pid, $interdiff_stdout, $interdiff_stderr, $use_select);
    if ($ENV{MOD_PERL}) {
        require Apache2::RequestUtil;
        require Apache2::SubProcess;
        my $request = Apache2::RequestUtil->request;
        (undef, $interdiff_stdout, $interdiff_stderr) = $request->spawn_proc_prog(
            $lc->{interdiffbin}, [$old_filename, $new_filename]
        );
        $use_select = !PERLIO_IS_ENABLED;
    } else {
        $interdiff_stderr = gensym;
        $pid = open3(gensym, $interdiff_stdout, $interdiff_stderr,
                        $lc->{interdiffbin}, $old_filename, $new_filename);
        $use_select = 1;
    }

    if ($format ne 'raw' && Bugzilla->params->{'utf8'}) {
        binmode $interdiff_stdout, ':utf8';
        binmode $interdiff_stderr, ':utf8';
    } else {
        binmode $interdiff_stdout;
        binmode $interdiff_stderr;
    }

    if ($use_select) {
        my $select = IO::Select->new();
        $select->add($interdiff_stdout, $interdiff_stderr);
        while (my @handles = $select->can_read) {
            foreach my $handle (@handles) {
                my $line = <$handle>;
                if (!defined $line) {
                    $select->remove($handle);
                    next;
                }
                if ($handle == $interdiff_stdout) {
                    $stdout .= $line;
                } else {
                    $stderr .= $line;
                }
            }
        }
        waitpid($pid, 0) if $pid;

    } else {
        local $/ = undef;
        $stdout = <$interdiff_stdout>;
        $stdout //= '';
        $stderr = <$interdiff_stderr>;
        $stderr //= '';
    }

    close($interdiff_stdout),
    close($interdiff_stderr);

    # Tidy up
    unlink($old_filename) or warn "Could not unlink $old_filename: $!";
    unlink($new_filename) or warn "Could not unlink $new_filename: $!";

    # Any output on STDERR means interdiff failed to full process the patches.
    # Interdiff's error messages are generic and not useful to end users, so we
    # show a generic failure message.
    if ($stderr) {
        warn($stderr);
        $warning = 'interdiff3';
    }

    my $reader = new PatchReader::Raw;

    if ($format eq 'raw') {
        require PatchReader::DiffPrinter::raw;
        $reader->sends_data_to(new PatchReader::DiffPrinter::raw());
        # Actually print out the patch.
        print $cgi->header(-type => 'text/plain');
        disable_utf8();
    }
    else {
        $vars->{'warning'} = $warning if $warning;
        $vars->{'bugid'} = $new_attachment->bug_id;
        $vars->{'oldid'} = $old_attachment->id;
        $vars->{'old_desc'} = $old_attachment->description;
        $vars->{'newid'} = $new_attachment->id;
        $vars->{'new_desc'} = $new_attachment->description;

        setup_template_patch_reader($reader, $vars);
    }
    $reader->iterate_string('interdiff #' . $old_attachment->id .
                            ' #' . $new_attachment->id, $stdout);
}

######################
#  Internal routines
######################

sub get_unified_diff {
    my ($attachment, $format) = @_;

    # Bring in the modules we need.
    require PatchReader::Raw;
    require PatchReader::DiffPrinter::raw;
    require PatchReader::PatchInfoGrabber;
    require File::Temp;

    $attachment->ispatch
      || ThrowCodeError('must_be_patch', { 'attach_id' => $attachment->id });

    # Reads in the patch, converting to unified diff in a temp file.
    my $reader = new PatchReader::Raw;
    my $last_reader = $reader;

    # Grabs the patch file info.
    my $patch_info_grabber = new PatchReader::PatchInfoGrabber();
    $last_reader->sends_data_to($patch_info_grabber);
    $last_reader = $patch_info_grabber;

    # Prints out to temporary file.
    my ($fh, $filename) = File::Temp::tempfile();
    if ($format ne 'raw' && Bugzilla->params->{'utf8'}) {
        # The HTML page will be displayed with the UTF-8 encoding.
        binmode $fh, ':utf8';
    }
    my $raw_printer = new PatchReader::DiffPrinter::raw($fh);
    $last_reader->sends_data_to($raw_printer);
    $last_reader = $raw_printer;

    # Iterate!
    $reader->iterate_string($attachment->id, $attachment->data);

    return ($filename, $patch_info_grabber->patch_info()->{files});
}

sub warn_if_interdiff_might_fail {
    my ($old_file_list, $new_file_list) = @_;

    # Verify that the list of files diffed is the same.
    my @old_files = sort keys %{$old_file_list};
    my @new_files = sort keys %{$new_file_list};
    if (@old_files != @new_files
        || join(' ', @old_files) ne join(' ', @new_files))
    {
        return 'interdiff1';
    }

    # Verify that the revisions in the files are the same.
    foreach my $file (keys %{$old_file_list}) {
        if (exists $old_file_list->{$file}{old_revision}
            && exists $new_file_list->{$file}{old_revision}
            && $old_file_list->{$file}{old_revision} ne
            $new_file_list->{$file}{old_revision})
        {
            return 'interdiff2';
        }
    }
    return undef;
}

sub setup_template_patch_reader {
    my ($last_reader, $vars) = @_;
    my $cgi = Bugzilla->cgi;
    my $template = Bugzilla->template;

    require PatchReader::DiffPrinter::template;

    # Define the vars for templates.
    if (defined $cgi->param('headers')) {
        $vars->{'headers'} = $cgi->param('headers');
    }
    else {
        $vars->{'headers'} = 1;
    }

    $vars->{'collapsed'} = $cgi->param('collapsed');

    # Print everything out.
    print $cgi->header(-type => 'text/html');

    $last_reader->sends_data_to(new PatchReader::DiffPrinter::template($template,
                                'attachment/diff-header.html.tmpl',
                                'attachment/diff-file.html.tmpl',
                                'attachment/diff-footer.html.tmpl',
                                $vars));
}

1;

__END__

=head1 B<Methods in need of POD>

=over

=item get_unified_diff

=item process_diff

=item warn_if_interdiff_might_fail

=item setup_template_patch_reader

=item process_interdiff

=back

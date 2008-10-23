#!/usr/bin/perl -wT
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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Myk Melez <myk@mozilla.org>
#                 Daniel Raichle <draichle@gmx.net>
#                 Dave Miller <justdave@syndicomm.com>
#                 Alexander J. Vincent <ajvincent@juno.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(.);

use vars qw(
  $template
  $vars
);

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";
use Bugzilla::Config qw(:locations);

# Use these modules to handle flags.
use Bugzilla::Constants;
use Bugzilla::Flag; 
use Bugzilla::FlagType; 
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Bug;

use IPC::Open2;

Bugzilla->login();

my $cgi = Bugzilla->cgi;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose
# value determines what the user wants to do.  The code below checks
# the value of that variable and runs the appropriate code. If none is
# supplied, we default to 'view'.

# Determine whether to use the action specified by the user or the default.
my $action = $cgi->param('action') || 'view';

if ($action eq "view")  
{
    view();
}
elsif ($action eq "interdiff")
{
    interdiff();
}
elsif ($action eq "diff")
{
    diff();
}
elsif ($action eq "viewall") 
{ 
    viewall(); 
}
elsif ($action eq "enter") 
{ 
    Bugzilla->login(LOGIN_REQUIRED);
    enter(); 
}
elsif ($action eq "insert")
{
    Bugzilla->login(LOGIN_REQUIRED);
    insert();
}
elsif ($action eq "edit") 
{ 
    edit(); 
}
elsif ($action eq "review")
{
    edit("review");
}
elsif ($action eq "reviewform")
{
    edit("reviewform");
}
elsif ($action eq "update") 
{ 
    Bugzilla->login(LOGIN_REQUIRED);
    update();
}
elsif ($action eq "prettypatch")
{
    prettyPatch();
}
else 
{ 
  ThrowCodeError("unknown_action", { action => $action });
}

exit;

################################################################################
# Data Validation / Security Authorization
################################################################################

# Validates an attachment ID. Optionally takes a parameter of a form
# variable name that contains the ID to be validated. If not specified,
# uses 'id'.
# 
# Will throw an error if 1) attachment ID is not a valid number,
# 2) attachment does not exist, or 3) user isn't allowed to access the
# attachment.
#
# Returns a list, where the first item is the validated, detainted
# attachment id, and the 2nd item is the bug id corresponding to the
# attachment.
# 
sub validateID
{
    my $param = @_ ? $_[0] : 'id';

    # If we're not doing interdiffs, check if id wasn't specified and
    # prompt them with a page that allows them to choose an attachment.
    # Happens when calling plain attachment.cgi from the urlbar directly
    if ($param eq 'id' && !$cgi->param('id')) {

        print Bugzilla->cgi->header();
        $template->process("attachment/choose.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
        exit;
    }
    
    my $attach_id = $cgi->param($param);

    # Validate the specified attachment id. detaint kills $attach_id if
    # non-natural, so use the original value from $cgi in our exception
    # message here.
    detaint_natural($attach_id)
     || ThrowUserError("invalid_attach_id", { attach_id => $cgi->param($param) });
  
    # Make sure the attachment exists in the database.
    SendSQL("SELECT bug_id, isprivate FROM attachments WHERE attach_id = $attach_id");
    MoreSQLData()
      || ThrowUserError("invalid_attach_id", { attach_id => $attach_id });

    # Make sure the user is authorized to access this attachment's bug.
    (my $bugid, my $isprivate) = FetchSQLData();

    ValidateBugID($bugid);
    if ($isprivate && Param("insidergroup")) {
        UserInGroup(Param("insidergroup"))
          || ThrowUserError("auth_failure", {action => "access",
                                             object => "attachment"});
    }

    return ($attach_id,$bugid);
}

# Validates format of a diff/interdiff. Takes a list as an parameter, which
# defines the valid format values. Will throw an error if the format is not
# in the list. Returns either the user selected or default format.
sub validateFormat
{
  # receives a list of legal formats; first item is a default
  my $format = $cgi->param('format') || $_[0];
  if ( lsearch(\@_, $format) == -1)
  {
     ThrowUserError("invalid_format", { format  => $format, formats => \@_ });
  }

  return $format;
}

# Validates context of a diff/interdiff. Will throw an error if the context
# is not number, "file" or "patch". Returns the validated, detainted context.
sub validateContext
{
  my $context = $cgi->param('context') || "patch";
  if ($context ne "file" && $context ne "patch") {
    detaint_natural($context)
      || ThrowUserError("invalid_context", { context => $cgi->param('context') });
  }

  return $context;
}

sub validateCanEdit
{
    my ($attach_id) = (@_);

    # People in editbugs can edit all attachments
    return if UserInGroup("editbugs");

    # Bug 97729 - the submitter can edit their attachments
    SendSQL("SELECT attach_id FROM attachments WHERE " .
            "attach_id = $attach_id AND submitter_id = " . Bugzilla->user->id);

    FetchSQLData()
      || ThrowUserError("illegal_attachment_edit",
                        { attach_id => $attach_id });
}

sub validateCanChangeAttachment 
{
    my ($attachid) = @_;
    SendSQL("SELECT product_id
             FROM attachments
             INNER JOIN bugs
             ON bugs.bug_id = attachments.bug_id
             WHERE attach_id = $attachid");
    my $productid = FetchOneColumn();
    CanEditProductId($productid)
      || ThrowUserError("illegal_attachment_edit",
                        { attach_id => $attachid });
}

sub validateCanChangeBug
{
    my ($bugid) = @_;
    SendSQL("SELECT product_id
             FROM bugs 
             WHERE bug_id = $bugid");
    my $productid = FetchOneColumn();
    CanEditProductId($productid)
      || ThrowUserError("illegal_attachment_edit_bug",
                        { bug_id => $bugid });
}

sub validateDescription
{
    $cgi->param('description')
      || ThrowUserError("missing_attachment_description");
}

sub validateIsPatch
{
    # Set the ispatch flag to zero if it is undefined, since the UI uses
    # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
    # do not get sent in HTML requests.
    $cgi->param('ispatch', $cgi->param('ispatch') ? 1 : 0);

    # Set the content type to text/plain if the attachment is a patch.
    $cgi->param('contenttype', 'text/plain') if $cgi->param('ispatch');
}

sub validateContentType
{
  if (!defined $cgi->param('contenttypemethod'))
  {
    ThrowUserError("missing_content_type_method");
  }
  elsif ($cgi->param('contenttypemethod') eq 'autodetect')
  {
    my $contenttype = $cgi->uploadInfo($cgi->param('data'))->{'Content-Type'};
    # The user asked us to auto-detect the content type, so use the type
    # specified in the HTTP request headers.
    if ( !$contenttype )
    {
      ThrowUserError("missing_content_type");
    }
    $cgi->param('contenttype', $contenttype);
  }
  elsif ($cgi->param('contenttypemethod') eq 'list')
  {
    # The user selected a content type from the list, so use their selection.
    $cgi->param('contenttype', $cgi->param('contenttypeselection'));
  }
  elsif ($cgi->param('contenttypemethod') eq 'manual')
  {
    # The user entered a content type manually, so use their entry.
    $cgi->param('contenttype', $cgi->param('contenttypeentry'));
  }
  else
  {
    ThrowCodeError("illegal_content_type_method",
                   { contenttypemethod => $cgi->param('contenttypemethod') });
  }

  if ( $cgi->param('contenttype') !~
         /^(application|audio|image|message|model|multipart|text|video)\/.+$/ )
  {
    ThrowUserError("invalid_content_type",
                   { contenttype => $cgi->param('contenttype') });
  }
}

sub validateIsObsolete
{
    # Set the isobsolete flag to zero if it is undefined, since the UI uses
    # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
    # do not get sent in HTML requests.
    $cgi->param('isobsolete', $cgi->param('isobsolete') ? 1 : 0);
}

sub validatePrivate
{
    # Set the isprivate flag to zero if it is undefined, since the UI uses
    # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
    # do not get sent in HTML requests.
    $cgi->param('isprivate', $cgi->param('isprivate') ? 1 : 0);
}

sub validateData
{
  my $maxsize = $cgi->param('ispatch') ? Param('maxpatchsize') : Param('maxattachmentsize');
  $maxsize *= 1024; # Convert from K
  my $fh;
  # Skip uploading into a local variable if the user wants to upload huge
  # attachments into local files.
  if (!$cgi->param('bigfile'))
  {
    $fh = $cgi->upload('data');
  }
  my $data;

  # We could get away with reading only as much as required, except that then
  # we wouldn't have a size to print to the error handler below.
  if (!$cgi->param('bigfile'))
  {
      # enable 'slurp' mode
      local $/;
      $data = <$fh>;
  }

  $data
    || ($cgi->param('bigfile'))
    || ThrowUserError("zero_length_file");

  # Make sure the attachment does not exceed the maximum permitted size
  my $len = $data ? length($data) : 0;
  if ($maxsize && $len > $maxsize) {
      my $vars = { filesize => sprintf("%.0f", $len/1024) };
      if ($cgi->param('ispatch')) {
          ThrowUserError("patch_too_large", $vars);
      } else {
          ThrowUserError("file_too_large", $vars);
      }
  }

  return $data || '';
}

sub validateFilename
{
  defined $cgi->upload('data')
    || ThrowUserError("file_not_specified");

  my $filename = $cgi->upload('data');
  
  # Remove path info (if any) from the file name.  The browser should do this
  # for us, but some are buggy.  This may not work on Mac file names and could
  # mess up file names with slashes in them, but them's the breaks.  We only
  # use this as a hint to users downloading attachments anyway, so it's not 
  # a big deal if it munges incorrectly occasionally.
  $filename =~ s/^.*[\/\\]//;

  # Truncate the filename to 100 characters, counting from the end of the string
  # to make sure we keep the filename extension.
  $filename = substr($filename, -100, 100);

  return $filename;
}

sub validateObsolete
{
  my @obsolete_ids = ();

  # Make sure the attachment id is valid and the user has permissions to view
  # the bug to which it is attached.
  foreach my $attachid ($cgi->param('obsolete')) {
    my $vars = {};
    $vars->{'attach_id'} = $attachid;
    
    detaint_natural($attachid)
      || ThrowCodeError("invalid_attach_id_to_obsolete", $vars);
  
    SendSQL("SELECT bug_id, isobsolete, description 
             FROM attachments WHERE attach_id = $attachid");

    # Make sure the attachment exists in the database.
    MoreSQLData()
      || ThrowUserError("invalid_attach_id", $vars);

    my ($bugid, $isobsolete, $description) = FetchSQLData();

    $vars->{'description'} = $description;
    
    if ($bugid != $cgi->param('bugid'))
    {
      $vars->{'my_bug_id'} = $cgi->param('bugid');
      $vars->{'attach_bug_id'} = $bugid;
      ThrowCodeError("mismatched_bug_ids_on_obsolete", $vars);
    }

    if ( $isobsolete )
    {
      ThrowCodeError("attachment_already_obsolete", $vars);
    }

    # Check that the user can modify this attachment
    validateCanEdit($attachid);
    push(@obsolete_ids, $attachid);
  }

  return @obsolete_ids;
}

# Returns 1 if the parameter is a content-type viewable in this browser
# Note that we don't use $cgi->Accept()'s ability to check if a content-type
# matches, because this will return a value even if it's matched by the generic
# */* which most browsers add to the end of their Accept: headers.
sub isViewable
{
  my $contenttype = trim(shift);
    
  # We assume we can view all text and image types  
  if ($contenttype =~ /^(text|image)\//) {
    return 1;
  }
  
  # Mozilla can view XUL. Note the trailing slash on the Gecko detection to
  # avoid sending XUL to Safari.
  if (($contenttype =~ /^application\/vnd\.mozilla\./) &&
      ($cgi->user_agent() =~ /Gecko\//))
  {
    return 1;
  }

  # If it's not one of the above types, we check the Accept: header for any 
  # types mentioned explicitly.
  my $accept = join(",", $cgi->Accept());
  
  if ($accept =~ /^(.*,)?\Q$contenttype\E(,.*)?$/) {
    return 1;
  }
  
  return 0;
}

################################################################################
# Functions
################################################################################

# Display an attachment.
sub view
{
    # Retrieve and validate parameters
    my ($attach_id) = validateID();

    # Retrieve the attachment content and its content type from the database.
    SendSQL("SELECT mimetype, filename, thedata FROM attachments " .
            "WHERE attach_id = $attach_id");
    my ($contenttype, $filename, $thedata) = FetchSQLData();
   
    # Bug 111522: allow overriding content-type manually in the posted form
    # params.
    if (defined $cgi->param('content_type'))
    {
        $cgi->param('contenttypemethod', 'manual');
        $cgi->param('contenttypeentry', $cgi->param('content_type'));
        validateContentType();
        $contenttype = $cgi->param('content_type');
    }

    # Return the appropriate HTTP response headers.
    $filename =~ s/^.*[\/\\]//;
    my $filesize = length($thedata);
    # A zero length attachment in the database means the attachment is 
    # stored in a local file
    if ($filesize == 0)
    {
        my $hash = ($attach_id % 100) + 100;
        $hash =~ s/.*(\d\d)$/group.$1/;
        if (open(AH, "$attachdir/$hash/attachment.$attach_id")) {
            binmode AH;
            $filesize = (stat(AH))[7];
        }
    }
    if ($filesize == 0)
    {
        ThrowUserError("attachment_removed");
    }


    # escape quotes and backslashes in the filename, per RFCs 2045/822
    $filename =~ s/\\/\\\\/g; # escape backslashes
    $filename =~ s/"/\\"/g; # escape quotes

    print Bugzilla->cgi->header(-type=>"$contenttype; name=\"$filename\"",
                                -content_disposition=> "inline; filename=\"$filename\"",
                                -content_length => $filesize);

    if ($thedata) {
        print $thedata;
    } else {
        while (<AH>) {
            print $_;
        }
        close(AH);
    }

}

sub interdiff
{
  # Retrieve and validate parameters
  my ($old_id) = validateID('oldid');
  my ($new_id) = validateID('newid');
  my $format = validateFormat('html', 'raw');
  my $context = validateContext();

  # Get old patch data
  my ($old_bugid, $old_description, $old_filename, $old_file_list) =
      get_unified_diff($old_id);

  # Get new patch data
  my ($new_bugid, $new_description, $new_filename, $new_file_list) =
      get_unified_diff($new_id);

  my $warning = warn_if_interdiff_might_fail($old_file_list, $new_file_list);

  #
  # send through interdiff, send output directly to template
  #
  # Must hack path so that interdiff will work.
  #
  $ENV{'PATH'} = $::diffpath;
  open my $interdiff_fh, "$::interdiffbin $old_filename $new_filename|";
  binmode $interdiff_fh;
    my ($reader, $last_reader) = setup_patch_readers("", $context);
    if ($format eq 'raw')
  {
    require PatchReader::DiffPrinter::raw;
    $last_reader->sends_data_to(new PatchReader::DiffPrinter::raw());
    # Actually print out the patch
    print $cgi->header(-type => 'text/plain',
                       -expires => '+3M');
  }
  else
  {
    $vars->{warning} = $warning if $warning;
    $vars->{bugid} = $new_bugid;
    $vars->{oldid} = $old_id;
    $vars->{old_desc} = $old_description;
    $vars->{newid} = $new_id;
    $vars->{new_desc} = $new_description;
    delete $vars->{attachid};
    delete $vars->{do_context};
    delete $vars->{context};
    setup_template_patch_reader($last_reader, $format, $context);
  }
  $reader->iterate_fh($interdiff_fh, "interdiff #$old_id #$new_id");
  close $interdiff_fh;
  $ENV{'PATH'} = '';

  #
  # Delete temporary files
  #
  unlink($old_filename) or warn "Could not unlink $old_filename: $!";
  unlink($new_filename) or warn "Could not unlink $new_filename: $!";
}

sub get_unified_diff
{
  my ($id) = @_;

  # Bring in the modules we need
  require PatchReader::Raw;
  require PatchReader::FixPatchRoot;
  require PatchReader::DiffPrinter::raw;
  require PatchReader::PatchInfoGrabber;
  require File::Temp;

  # Get the patch
  SendSQL("SELECT bug_id, description, ispatch, thedata FROM attachments WHERE attach_id = $id");
  my ($bugid, $description, $ispatch, $thedata) = FetchSQLData();
  if (!$ispatch) {
    $vars->{'attach_id'} = $id;
    ThrowUserError("must_be_patch");
  }

  # Reads in the patch, converting to unified diff in a temp file
  my $reader = new PatchReader::Raw;
  my $last_reader = $reader;

  # fixes patch root (makes canonical if possible)
  if (Param('cvsroot')) {
    my $fix_patch_root = new PatchReader::FixPatchRoot(Param('cvsroot'));
    $last_reader->sends_data_to($fix_patch_root);
    $last_reader = $fix_patch_root;
  }

  # Grabs the patch file info
  my $patch_info_grabber = new PatchReader::PatchInfoGrabber();
  $last_reader->sends_data_to($patch_info_grabber);
  $last_reader = $patch_info_grabber;

  # Prints out to temporary file
  my ($fh, $filename) = File::Temp::tempfile();
  my $raw_printer = new PatchReader::DiffPrinter::raw($fh);
  $last_reader->sends_data_to($raw_printer);
  $last_reader = $raw_printer;

  # Iterate!
  $reader->iterate_string($id, $thedata);

  return ($bugid, $description, $filename, $patch_info_grabber->patch_info()->{files});
}

sub warn_if_interdiff_might_fail {
  my ($old_file_list, $new_file_list) = @_;
  # Verify that the list of files diffed is the same
  my @old_files = sort keys %{$old_file_list};
  my @new_files = sort keys %{$new_file_list};
  if (@old_files != @new_files ||
      join(' ', @old_files) ne join(' ', @new_files)) {
    return "interdiff1";
  }

  # Verify that the revisions in the files are the same
  foreach my $file (keys %{$old_file_list}) {
    if ($old_file_list->{$file}{old_revision} ne
        $new_file_list->{$file}{old_revision}) {
      return "interdiff2";
    }
  }

  return undef;
}

sub setup_patch_readers {
  my ($diff_root, $context) = @_;

  #
  # Parameters:
  # format=raw|html
  # context=patch|file|0-n
  # collapsed=0|1
  # headers=0|1
  #

  # Define the patch readers
  # The reader that reads the patch in (whatever its format)
  require PatchReader::Raw;
  my $reader = new PatchReader::Raw;
  my $last_reader = $reader;
  # Fix the patch root if we have a cvs root
  if (Param('cvsroot'))
  {
    require PatchReader::FixPatchRoot;
    $last_reader->sends_data_to(new PatchReader::FixPatchRoot(Param('cvsroot')));
    $last_reader->sends_data_to->diff_root($diff_root) if defined($diff_root);
    $last_reader = $last_reader->sends_data_to;
  }
  # Add in cvs context if we have the necessary info to do it
  if ($context ne "patch" && $::cvsbin && Param('cvsroot_get'))
  {
    require PatchReader::AddCVSContext;
    $last_reader->sends_data_to(
          new PatchReader::AddCVSContext($context,
                                         Param('cvsroot_get')));
    $last_reader = $last_reader->sends_data_to;
  }
  return ($reader, $last_reader);
}

sub setup_template_patch_reader
{
  my ($last_reader, $format, $context) = @_;

  require PatchReader::DiffPrinter::template;

  # Define the vars for templates
  if (defined $cgi->param('headers')) {
    $vars->{headers} = $cgi->param('headers');
  } else {
    $vars->{headers} = 1 if !defined $cgi->param('headers');
  }
  $vars->{collapsed} = $cgi->param('collapsed');
  $vars->{context} = $context;
  $vars->{do_context} = $::cvsbin && Param('cvsroot_get') && !$vars->{'newid'};

  # Print everything out
  print $cgi->header(-type => 'text/html',
                     -expires => '+3M');
  $last_reader->sends_data_to(new PatchReader::DiffPrinter::template($template,
                             "attachment/diff-header.$format.tmpl",
                             "attachment/diff-file.$format.tmpl",
                             "attachment/diff-footer.$format.tmpl",
                             { %{$vars},
                               bonsai_url => Param('bonsai_url'),
                               lxr_url => Param('lxr_url'),
                               lxr_root => Param('lxr_root'),
                             }));
}

sub prettyPatch
{
  # Retrieve and validate parameters
  my ($attach_id) = validateID();
  my $format = validateFormat('html', 'raw');
  my $context = validateContext();

  # Get patch data
  SendSQL("SELECT bug_id, description, ispatch, thedata FROM attachments " .
          "WHERE attach_id = $attach_id");
  my ($bugid, $description, $ispatch, $thedata) = FetchSQLData();

  # If it is not a patch, view normally
  if (!$ispatch)
  {
    view();
    return;
  }

  use vars qw($cgi);
  print $cgi->header(-type => 'text/html',
                     -expires => '+3M');

  open2(\*OUT, \*IN, "/usr/bin/ruby", "-I", "PrettyPatch", "PrettyPatch/prettify.rb", "--html-exceptions");
  print IN $thedata . "\n";
  close(IN);
  while (<OUT>) {
      print;
  }
  close(OUT);
}

sub diff
{
  # Retrieve and validate parameters
  my ($attach_id) = validateID();
  my $format = validateFormat('html', 'raw');
  my $context = validateContext();

  # Get patch data
  SendSQL("SELECT bug_id, description, ispatch, thedata FROM attachments " .
          "WHERE attach_id = $attach_id");
  my ($bugid, $description, $ispatch, $thedata) = FetchSQLData();

  # If it is not a patch, view normally
  if (!$ispatch)
  {
    view();
    return;
  }

  my ($reader, $last_reader) = setup_patch_readers(undef,$context);

  if ($format eq 'raw')
  {
    require PatchReader::DiffPrinter::raw;
    $last_reader->sends_data_to(new PatchReader::DiffPrinter::raw());
    # Actually print out the patch
    use vars qw($cgi);
    print $cgi->header(-type => 'text/plain',
                       -expires => '+3M');
    $reader->iterate_string("Attachment $attach_id", $thedata);
  }
  else
  {
    $vars->{other_patches} = [];
    if ($::interdiffbin && $::diffpath) {
      # Get list of attachments on this bug.
      # Ignore the current patch, but select the one right before it
      # chronologically.
      SendSQL("SELECT attach_id, description FROM attachments WHERE bug_id = $bugid AND ispatch = 1 ORDER BY creation_ts DESC");
      my $select_next_patch = 0;
      while (my ($other_id, $other_desc) = FetchSQLData()) {
        if ($other_id eq $attach_id) {
          $select_next_patch = 1;
        } else {
          push @{$vars->{other_patches}}, { id => $other_id, desc => $other_desc, selected => $select_next_patch };
          if ($select_next_patch) {
            $select_next_patch = 0;
          }
        }
      }
    }

    $vars->{bugid} = $bugid;
    $vars->{attachid} = $attach_id;
    $vars->{description} = $description;
    setup_template_patch_reader($last_reader, $format, $context);
    # Actually print out the patch
    $reader->iterate_string("Attachment $attach_id", $thedata);
  }
}

# Display all attachments for a given bug in a series of IFRAMEs within one
# HTML page.
sub viewall
{
    # Retrieve and validate parameters
    my $bugid = $cgi->param('bugid');
    ValidateBugID($bugid);

    # Retrieve the attachments from the database and write them into an array
    # of hashes where each hash represents one attachment.
    my $privacy = "";
    my $dbh = Bugzilla->dbh;

    if (Param("insidergroup") && !(UserInGroup(Param("insidergroup")))) {
        $privacy = "AND isprivate < 1 ";
    }
    SendSQL("SELECT attach_id, " .
            $dbh->sql_date_format('creation_ts', '%Y.%m.%d %H:%i') . ",
            mimetype, description, ispatch, isobsolete, isprivate, 
            LENGTH(thedata)
            FROM attachments WHERE bug_id = $bugid $privacy 
            ORDER BY attach_id");
  my @attachments; # the attachments array
  while (MoreSQLData())
  {
    my %a; # the attachment hash
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'},
     $a{'description'}, $a{'ispatch'}, $a{'isobsolete'}, $a{'isprivate'},
     $a{'datasize'}) = FetchSQLData();
    $a{'isviewable'} = isViewable($a{'contenttype'});
    $a{'flags'} = Bugzilla::Flag::match({ 'attach_id' => $a{'attachid'},
                                          'is_active' => 1 });

    # Add the hash representing the attachment to the array of attachments.
    push @attachments, \%a;
  }

  # Retrieve the bug summary (for displaying on screen) and assignee.
  SendSQL("SELECT short_desc, assigned_to FROM bugs " .
          "WHERE bug_id = $bugid");
  my ($bugsummary, $assignee_id) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'bugid'} = $bugid;
  $vars->{'attachments'} = \@attachments;
  $vars->{'bugassignee_id'} = $assignee_id;
  $vars->{'bugsummary'} = $bugsummary;
  $vars->{'GetBugLink'} = \&GetBugLink;

  print Bugzilla->cgi->header();

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/show-multiple.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}

# Display a form for entering a new attachment.
sub enter
{
  # Retrieve and validate parameters
  my $bugid = $cgi->param('bugid');
  ValidateBugID($bugid);
  validateCanChangeBug($bugid);

  # Retrieve the attachments the user can edit from the database and write
  # them into an array of hashes where each hash represents one attachment.
  my $canEdit = "";
  if (!UserInGroup("editbugs")) {
      $canEdit = "AND submitter_id = " . Bugzilla->user->id;
  }
  SendSQL("SELECT attach_id, description, isprivate
           FROM attachments
           WHERE bug_id = $bugid 
           AND isobsolete = 0 $canEdit
           ORDER BY attach_id");
  my @attachments; # the attachments array
  while ( MoreSQLData() ) {
    my %a; # the attachment hash
    ($a{'id'}, $a{'description'}, $a{'isprivate'}) = FetchSQLData();

    # Add the hash representing the attachment to the array of attachments.
    push @attachments, \%a;
  }

  # Retrieve the bug summary (for displaying on screen) and assignee.
  SendSQL("SELECT short_desc, assigned_to FROM bugs 
           WHERE bug_id = $bugid");
  my ($bugsummary, $assignee_id) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'bugid'} = $bugid;
  $vars->{'attachments'} = \@attachments;
  $vars->{'bugassignee_id'} = $assignee_id;
  $vars->{'bugsummary'} = $bugsummary;
  $vars->{'GetBugLink'} = \&GetBugLink;

  SendSQL("SELECT product_id, component_id FROM bugs
           WHERE bug_id = $bugid");
  my ($product_id, $component_id) = FetchSQLData();
  my $flag_types = Bugzilla::FlagType::match({'target_type'  => 'attachment',
                                              'product_id'   => $product_id,
                                              'component_id' => $component_id});
  $vars->{'flag_types'} = $flag_types;
  $vars->{'any_flags_requesteeble'} = grep($_->{'is_requesteeble'},
                                           @$flag_types);
  $vars->{'bug'} = new Bugzilla::Bug($bugid, Bugzilla->user);

  print Bugzilla->cgi->header();

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/create.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}

# Insert a new attachment into the database.
sub insert
{
    my $dbh = Bugzilla->dbh;
    my $userid = Bugzilla->user->id;

    # Retrieve and validate parameters
    my $bugid = $cgi->param('bugid');
    ValidateBugID($bugid);
    validateCanChangeBug($bugid);
    ValidateComment(scalar $cgi->param('comment'));
    my $filename = validateFilename();
    validateIsPatch();
    my $data = validateData();
    validateDescription();
    validateContentType() unless $cgi->param('ispatch');

    my @obsolete_ids = ();
    @obsolete_ids = validateObsolete() if $cgi->param('obsolete');

    # The order of these function calls is important, as both Flag::validate
    # and FlagType::validate assume User::match_field has ensured that the
    # values in the requestee fields are legitimate user email addresses.
    my $match_status = Bugzilla::User::match_field($cgi, {
        '^requestee(_type)?-(\d+)$' => { 'type' => 'single' },
    }, MATCH_SKIP_CONFIRM);

    $vars->{'match_field'} = 'requestee';
    if ($match_status == USER_MATCH_FAILED) {
        $vars->{'message'} = 'user_match_failed';
    }
    elsif ($match_status == USER_MATCH_MULTIPLE) {
        $vars->{'message'} = 'user_match_multiple';
    }

    # FlagType::validate() and Flag::validate() should not detect
    # any reference to existing flags when creating a new attachment.
    # Setting the third param to -1 will force this function to check this point.
    Bugzilla::Flag::validate($cgi, $bugid, -1);
    Bugzilla::FlagType::validate($cgi, $bugid, -1);

    # Escape characters in strings that will be used in SQL statements.
    my $sql_filename = SqlQuote($filename);
    my $description = SqlQuote($cgi->param('description'));
    my $contenttype = SqlQuote($cgi->param('contenttype'));
    my $isprivate = $cgi->param('isprivate') ? 1 : 0;

  # Figure out when the changes were made.
  my ($timestamp) = Bugzilla->dbh->selectrow_array("SELECT NOW()"); 
  my $sql_timestamp = SqlQuote($timestamp); 

  # Insert the attachment into the database.
  my $sth = $dbh->prepare("INSERT INTO attachments
      (thedata, bug_id, creation_ts, filename, description,
       mimetype, ispatch, isprivate, submitter_id) 
      VALUES (?, $bugid, $sql_timestamp, $sql_filename,
              $description, $contenttype, " . $cgi->param('ispatch') . ",
              $isprivate, $userid)");
  # We only use $data here in this INSERT with a placeholder,
  # so it's safe.
  trick_taint($data);
  $sth->bind_param(1, $data, $dbh->BLOB_TYPE);
  $sth->execute();

  # Retrieve the ID of the newly created attachment record.
  my $attachid = $dbh->bz_last_key('attachments', 'attach_id');

  # If the file is to be stored locally, stream the file from the webserver
  # to the local file without reading it into a local variable.
  if ($cgi->param('bigfile'))
  {
    my $fh = $cgi->upload('data');
    my $hash = ($attachid % 100) + 100;
    $hash =~ s/.*(\d\d)$/group.$1/;
    mkdir "$attachdir/$hash", 0770;
    chmod 0770, "$attachdir/$hash";
    open(AH, ">$attachdir/$hash/attachment.$attachid");
    binmode AH;
    my $sizecount = 0;
    my $limit = (Param("maxlocalattachment") * 1048576);
    while (<$fh>) {
        print AH $_;
        $sizecount += length($_);
        if ($sizecount > $limit) {
            close AH;
            close $fh;
            unlink "$attachdir/$hash/attachment.$attachid";
            ThrowUserError("local_file_too_large");
        }
    }
    close AH;
    close $fh;
  }


  # Insert a comment about the new attachment into the database.
  my $comment = "Created an attachment (id=$attachid)\n" .
                $cgi->param('description') . "\n";
  $comment .= ("\n" . $cgi->param('comment')) if defined $cgi->param('comment');

  AppendComment($bugid, $userid, $comment, $isprivate, $timestamp);

  # Make existing attachments obsolete.
  my $fieldid = GetFieldID('attachments.isobsolete');
  foreach my $obsolete_id (@obsolete_ids) {
      # If the obsolete attachment has request flags, cancel them.
      # This call must be done before updating the 'attachments' table.
      Bugzilla::Flag::CancelRequests($bugid, $obsolete_id, $sql_timestamp);

      SendSQL("UPDATE attachments SET isobsolete = 1 " . 
              "WHERE attach_id = $obsolete_id");
      SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                          fieldid, removed, added) 
              VALUES ($bugid, $obsolete_id, $userid, $sql_timestamp, $fieldid,
                      '0', '1')");
  }

  # Assign the bug to the user, if they are allowed to take it
  my $owner = "";
  
  if ($cgi->param('takebug') && UserInGroup("editbugs")) {
      
      my @fields = ("assigned_to", "bug_status", "resolution", "everconfirmed",
                    "login_name");
      
      # Get the old values, for the bugs_activity table
      SendSQL("SELECT " . join(", ", @fields) . " " .
              "FROM bugs " .
              "INNER JOIN profiles " .
              "ON profiles.userid = bugs.assigned_to " .
              "WHERE bugs.bug_id = $bugid");
      
      my @oldvalues = FetchSQLData();
      my @newvalues = ($userid, "ASSIGNED", "", 1, Bugzilla->user->login);
      
      # Make sure the person we are taking the bug from gets mail.
      $owner = $oldvalues[4];  
                  
      @oldvalues = map(SqlQuote($_), @oldvalues);
      @newvalues = map(SqlQuote($_), @newvalues);
               
      # Update the bug record. Note that this doesn't involve login_name.
      SendSQL("UPDATE bugs SET delta_ts = $sql_timestamp, " . 
              join(", ", map("$fields[$_] = $newvalues[$_]", (0..3))) . 
              " WHERE bug_id = $bugid");
      
      # We store email addresses in the bugs_activity table rather than IDs.
      $oldvalues[0] = $oldvalues[4];
      $newvalues[0] = $newvalues[4];
      
      # Add the changes to the bugs_activity table
      for (my $i = 0; $i < 4; $i++) {
          if ($oldvalues[$i] ne $newvalues[$i]) {
              my $fieldid = GetFieldID($fields[$i]);
              SendSQL("INSERT INTO bugs_activity " .
                      "(bug_id, who, bug_when, fieldid, removed, added) " .
                      "VALUES ($bugid, $userid, $sql_timestamp, " .
                      "$fieldid, $oldvalues[$i], $newvalues[$i])");
          }
      }      
  }   
  
  # Create flags.
  my $target = Bugzilla::Flag::GetTarget(undef, $attachid);
  Bugzilla::Flag::process($target, $timestamp, $cgi);
   
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'mailrecipients'} =  { 'changer' => Bugzilla->user->login,
                                 'owner'   => $owner };
  $vars->{'bugid'} = $bugid;
  $vars->{'attachid'} = $attachid;
  $vars->{'description'} = $description;
  $vars->{'contenttypemethod'} = $cgi->param('contenttypemethod');
  $vars->{'contenttype'} = $cgi->param('contenttype');

  print Bugzilla->cgi->header();

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/created.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}

# Displays a form for editing attachment properties.
# Any user is allowed to access this page, unless the attachment
# is private and the user does not belong to the insider group.
# Validations are done later when the user submits changes.
sub edit
{
  my ($template_name) = @_;
  $template_name = $template_name || "edit";

  # Retrieve and validate parameters
  my ($attach_id) = validateID();

  # Retrieve the attachment from the database.
  SendSQL("SELECT description, mimetype, filename, bug_id, ispatch, isobsolete, isprivate, thedata
           FROM attachments WHERE attach_id = $attach_id");
  my ($description, $contenttype, $filename, $bugid, $ispatch, $isobsolete, $isprivate, $thedata) = FetchSQLData();
  my $datasize = length($thedata);

  my $isviewable = isViewable($contenttype);

  # Retrieve a list of attachments for this bug as well as a summary of the bug
  # to use in a navigation bar across the top of the screen.
  SendSQL("SELECT attach_id FROM attachments WHERE bug_id = $bugid ORDER BY attach_id");
  my @bugattachments;
  push(@bugattachments, FetchSQLData()) while (MoreSQLData());
  SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $bugid");
  my ($bugsummary) = FetchSQLData();
  
  # Get a list of flag types that can be set for this attachment.
  SendSQL("SELECT product_id, component_id FROM bugs WHERE bug_id = $bugid");
  my ($product_id, $component_id) = FetchSQLData();
  my $flag_types = Bugzilla::FlagType::match({ 'target_type'  => 'attachment' ,
                                               'product_id'   => $product_id ,
                                               'component_id' => $component_id });
  foreach my $flag_type (@$flag_types) {
    $flag_type->{'flags'} = Bugzilla::Flag::match({ 'type_id'   => $flag_type->{'id'},
                                                    'attach_id' => $attach_id,
                                                    'is_active' => 1 });
  }
  $vars->{'flag_types'} = $flag_types;
  $vars->{'any_flags_requesteeble'} = grep($_->{'is_requesteeble'}, @$flag_types);
  
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'attachid'} = $attach_id; 
  $vars->{'description'} = $description; 
  $vars->{'contenttype'} = $contenttype; 
  $vars->{'filename'} = $filename;
  $vars->{'bugid'} = $bugid; 
  $vars->{'bugsummary'} = $bugsummary; 
  $vars->{'ispatch'} = $ispatch; 
  $vars->{'isobsolete'} = $isobsolete; 
  $vars->{'isprivate'} = $isprivate; 
  $vars->{'datasize'} = $datasize;
  $vars->{'isviewable'} = $isviewable; 
  $vars->{'attachments'} = \@bugattachments; 
  $vars->{'GetBugLink'} = \&GetBugLink;

  if ($ispatch) {
      my $quotedpatch = $thedata;
      $quotedpatch =~ s/^/> /mg;
      $vars->{'quotedpatch'} = $quotedpatch;
  }

  # Determine if PatchReader is installed
  eval {
    require PatchReader;
    $vars->{'patchviewerinstalled'} = 1;
  };
  print Bugzilla->cgi->header();

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/$template_name.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}

# Updates an attachment record. Users with "editbugs" privileges, (or the
# original attachment's submitter) can edit the attachment's description,
# content type, ispatch and isobsolete flags, and statuses, and they can
# also submit a comment that appears in the bug.
# Users cannot edit the content of the attachment itself.
sub update
{
  my $dbh = Bugzilla->dbh;
  my $userid = Bugzilla->user->id;

    # Retrieve and validate parameters
    ValidateComment(scalar $cgi->param('comment'));
    my ($attach_id, $bugid) = validateID();
    validateCanEdit($attach_id);
    validateCanChangeAttachment($attach_id);
    validateDescription();
    validateIsPatch();
    validateContentType() unless $cgi->param('ispatch');
    validateIsObsolete();
    validatePrivate();

    # The order of these function calls is important, as both Flag::validate
    # and FlagType::validate assume User::match_field has ensured that the
    # values in the requestee fields are legitimate user email addresses.
    Bugzilla::User::match_field($cgi, {
        '^requestee(_type)?-(\d+)$' => { 'type' => 'single' }
    });
    Bugzilla::Flag::validate($cgi, $bugid, $attach_id);
    Bugzilla::FlagType::validate($cgi, $bugid, $attach_id);

  # Lock database tables in preparation for updating the attachment.
  $dbh->bz_lock_tables('attachments WRITE', 'flags WRITE' ,
          'flagtypes READ', 'fielddefs READ', 'bugs_activity WRITE',
          'flaginclusions AS i READ', 'flagexclusions AS e READ',
          # cc, bug_group_map, user_group_map, and groups are in here so we
          # can check the permissions of flag requestees and email addresses
          # on the flag type cc: lists via the CanSeeBug
          # function call in Flag::notify. group_group_map is in here in case
          # Bugzilla::User needs to rederive groups. profiles and 
          # user_group_map would be READ locks instead of WRITE locks if it
          # weren't for derive_groups, which needs to write to those tables.
          'bugs WRITE', 'profiles WRITE', 'email_setting READ',
          'cc READ', 'bug_group_map READ', 'user_group_map WRITE',
          'group_group_map READ', 'groups READ');

  # Get a copy of the attachment record before we make changes
  # so we can record those changes in the activity table.
  SendSQL("SELECT description, mimetype, filename, ispatch, isobsolete, isprivate
           FROM attachments WHERE attach_id = $attach_id");
  my ($olddescription, $oldcontenttype, $oldfilename, $oldispatch,
      $oldisobsolete, $oldisprivate) = FetchSQLData();

  # Quote the description and content type for use in the SQL UPDATE statement.
  my $quoteddescription = SqlQuote($cgi->param('description'));
  my $quotedcontenttype = SqlQuote($cgi->param('contenttype'));
  my $quotedfilename = SqlQuote($cgi->param('filename'));

  # Figure out when the changes were made.
  SendSQL("SELECT NOW()");
  my $timestamp = FetchOneColumn();
    
  # Update flags.  We have to do this before committing changes
  # to attachments so that we can delete pending requests if the user
  # is obsoleting this attachment without deleting any requests
  # the user submits at the same time.
  my $target = Bugzilla::Flag::GetTarget(undef, $attach_id);
  Bugzilla::Flag::process($target, $timestamp, $cgi);

  # Update the attachment record in the database.
  SendSQL("UPDATE  attachments 
           SET     description = $quoteddescription ,
                   mimetype = $quotedcontenttype ,
                   filename = $quotedfilename ,
                   ispatch = " . $cgi->param('ispatch') . ",
                   isobsolete = " . $cgi->param('isobsolete') . ",
                   isprivate = " . $cgi->param('isprivate') . "
           WHERE   attach_id = $attach_id
         ");

  # Record changes in the activity table.
  my $sql_timestamp = SqlQuote($timestamp);
  if ($olddescription ne $cgi->param('description')) {
    my $quotedolddescription = SqlQuote($olddescription);
    my $fieldid = GetFieldID('attachments.description');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $quotedolddescription, $quoteddescription)");
  }
  if ($oldcontenttype ne $cgi->param('contenttype')) {
    my $quotedoldcontenttype = SqlQuote($oldcontenttype);
    my $fieldid = GetFieldID('attachments.mimetype');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $quotedoldcontenttype, $quotedcontenttype)");
  }
  if ($oldfilename ne $cgi->param('filename')) {
    my $quotedoldfilename = SqlQuote($oldfilename);
    my $fieldid = GetFieldID('attachments.filename');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $quotedoldfilename, $quotedfilename)");
  }
  if ($oldispatch ne $cgi->param('ispatch')) {
    my $fieldid = GetFieldID('attachments.ispatch');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $oldispatch, " . $cgi->param('ispatch') . ")");
  }
  if ($oldisobsolete ne $cgi->param('isobsolete')) {
    my $fieldid = GetFieldID('attachments.isobsolete');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $oldisobsolete, " . $cgi->param('isobsolete') . ")");
  }
  if ($oldisprivate ne $cgi->param('isprivate')) {
    my $fieldid = GetFieldID('attachments.isprivate');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when,
                                        fieldid, removed, added)
             VALUES ($bugid, $attach_id, $userid, $sql_timestamp, $fieldid,
                     $oldisprivate, " . $cgi->param('isprivate') . ")");
  }
  
  # Unlock all database tables now that we are finished updating the database.
  $dbh->bz_unlock_tables();

  # If the user submitted a comment while editing the attachment,
  # add the comment to the bug.
  if ($cgi->param('comment'))
  {
    # Prepend a string to the comment to let users know that the comment came
    # from the "edit attachment" screen.
    my $comment = qq|(From update of attachment $attach_id)\n| .
                  $cgi->param('comment');

    # Append the comment to the list of comments in the database.
    AppendComment($bugid, $userid, $comment, $cgi->param('isprivate'), $timestamp);
  }
  
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'mailrecipients'} = { 'changer' => Bugzilla->user->login };
  $vars->{'attachid'} = $attach_id; 
  $vars->{'bugid'} = $bugid; 

  print Bugzilla->cgi->header();

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/updated.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}

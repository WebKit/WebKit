#!/usr/bin/perl -T
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

# This script reads in xml bug data from standard input and inserts
# a new bug into bugzilla. Everything before the beginning <?xml line
# is removed so you can pipe in email messages.

use 5.10.1;
use strict;
use warnings;

#####################################################################
#
# This script is used to import bugs from another installation of bugzilla.
# It can be used in two ways.
# First using the move function of bugzilla
# on another system will send mail to an alias provided by
# the administrator of the target installation (you). Set up an alias
# similar to the one given below so this mail will be automatically
# run by this script and imported into your database.  Run 'newaliases'
# after adding this alias to your aliases file. Make sure your sendmail
# installation is configured to allow mail aliases to execute code.
#
# bugzilla-import: "|/usr/bin/perl /opt/bugzilla/importxml.pl"
#
# Second it can be run from the command line with any xml file from
# STDIN that conforms to the bugzilla DTD. In this case you can pass
# an argument to set whether you want to send the
# mail that will be sent to the exporter and maintainer normally.
#
# importxml.pl bugsfile.xml
#
#####################################################################

use File::Basename qw(dirname);
# MTAs may call this script from any directory, but it should always
# run from this one so that it can find its modules.
BEGIN {
    require File::Basename;
    my $dir = $0; $dir =~ /(.*)/; $dir = $1; # trick taint
    chdir(File::Basename::dirname($dir));
}

use lib qw(. lib);
# Data dumber is used for debugging, I got tired of copying it back in 
# and then removing it. 
#use Data::Dumper;


use Bugzilla;
use Bugzilla::Object;
use Bugzilla::Bug;
use Bugzilla::Attachment;
use Bugzilla::Product;
use Bugzilla::Version;
use Bugzilla::Component;
use Bugzilla::Milestone;
use Bugzilla::FlagType;
use Bugzilla::BugMail;
use Bugzilla::Mailer;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Keyword;
use Bugzilla::Field;
use Bugzilla::Status;

use MIME::Base64;
use MIME::Parser;
use Getopt::Long;
use Pod::Usage;
use XML::Twig;

my $debug = 0;
my $mail  = '';
my $attach_path = '';
my $help  = 0;
my $bug_page = 'show_bug.cgi?id=';
my $default_product_name = '';
my $default_component_name = '';

my $result = GetOptions(
    "verbose|debug+" => \$debug,
    "mail|sendmail!" => \$mail,
    "attach_path=s"  => \$attach_path,
    "help|?"         => \$help,
    "bug_page=s"     => \$bug_page,
    "product=s"      => \$default_product_name,
    "component=s"    => \$default_component_name,
);

pod2usage(0) if $help;

use constant OK_LEVEL    => 3;
use constant DEBUG_LEVEL => 2;
use constant ERR_LEVEL   => 1;

our @logs;
our @attachments;
our $bugtotal;
my $xml;
my $dbh = Bugzilla->dbh;
my $params = Bugzilla->params;
my ($timestamp) = $dbh->selectrow_array("SELECT NOW()");

###############################################################################
# Helper sub routines                                                         #
###############################################################################

sub MailMessage {
    return unless ($mail);
    my $subject    = shift;
    my $message    = shift;
    my @recipients = @_;
    my $from   = $params->{"mailfrom"};
    $from =~ s/@/\@/g;

    foreach my $to (@recipients){
        my $header = "To: $to\n";
        $header .= "From: Bugzilla <$from>\n";
        $header .= "Subject: $subject\n\n";
        my $sendmessage = $header . $message . "\n";
        MessageToMTA($sendmessage);
    }

}

sub Debug {
    return unless ($debug);
    my ( $message, $level ) = (@_);
    print STDERR "OK: $message \n" if ( $level == OK_LEVEL );
    print STDERR "ERR: $message \n" if ( $level == ERR_LEVEL );
    print STDERR "$message\n"
      if ( ( $debug == $level ) && ( $level == DEBUG_LEVEL ) );
}

sub Error {
    my ( $reason, $errtype, $exporter ) = @_;
    my $subject = "Bug import error: $reason";
    my $message = "Cannot import these bugs because $reason ";
    $message .= "\n\nPlease re-open the original bug.\n" if ($errtype);
    $message .= "For more info, contact " . $params->{"maintainer"} . ".\n";
    my @to = ( $params->{"maintainer"}, $exporter);
    Debug( $message, ERR_LEVEL );
    MailMessage( $subject, $message, @to );
    exit;
}

# This subroutine handles flags for process_bug. It is generic in that
# it can handle both attachment flags and bug flags.
sub flag_handler {
    my (
        $name,            $status,      $setter_login,
        $requestee_login, $exporterid,  $bugid,
        $componentid,     $productid,   $attachid
      )
      = @_;

    my $type         = ($attachid) ? "attachment" : "bug";
    my $err          = '';
    my $setter       = new Bugzilla::User({ name => $setter_login });
    my $requestee;
    my $requestee_id;

    unless ($setter) {
        $err = "Invalid setter $setter_login on $type flag $name\n";
        $err .= "   Dropping flag $name\n";
        return $err;
    }
    if ( !$setter->can_see_bug($bugid) ) {
        $err .= "Setter is not a member of bug group\n";
        $err .= "   Dropping flag $name\n";
        return $err;
    }
    my $setter_id = $setter->id;
    if ( defined($requestee_login) ) {
        $requestee = new Bugzilla::User({ name => $requestee_login });
        if ( $requestee ) {
            if ( !$requestee->can_see_bug($bugid) ) {
                $err .= "Requestee is not a member of bug group\n";
                $err .= "   Requesting from the wind\n";
            }    
            else{
                $requestee_id = $requestee->id;
            }
        }
        else {
            $err = "Invalid requestee $requestee_login on $type flag $name\n";
            $err .= "   Requesting from the wind.\n";
        }
        
    }
    my $flag_types;

    # If this is an attachment flag we need to do some dirty work to look
    # up the flagtype ID
    if ($attachid) {
        $flag_types = Bugzilla::FlagType::match(
            {
                'target_type'  => 'attachment',
                'product_id'   => $productid,
                'component_id' => $componentid
            } );
    }
    else {
        my $bug = new Bugzilla::Bug($bugid);
        $flag_types = $bug->flag_types;
    }
    unless ($flag_types){
        $err  = "No flag types defined for this bug\n";
        $err .= "   Dropping flag $name\n";
        return $err;
    }

    # We need to see if the imported flag is in the list of known flags
    # It is possible for two flags on the same bug have the same name
    # If this is the case, we will only match the first one.
    my $ftype;
    foreach my $f ( @{$flag_types} ) {
        if ( $f->name eq $name) {
            $ftype = $f;
            last;
        }
    }

    if ($ftype) {    # We found the flag in the list
        my $grant_group = $ftype->grant_group;
        if (( $status eq '+' || $status eq '-' ) 
            && $grant_group && !$setter->in_group_id($grant_group->id)) {
            $err = "Setter $setter_login on $type flag $name ";
            $err .= "is not in the Grant Group\n";
            $err .= "   Dropping flag $name\n";
            return $err;
        }
        my $request_group = $ftype->request_group;
        if ($request_group
            && $status eq '?' && !$setter->in_group_id($request_group->id)) {
            $err = "Setter $setter_login on $type flag $name ";
            $err .= "is not in the Request Group\n";
            $err .= "   Dropping flag $name\n";
            return $err;
        }

        # Take the first flag_type that matches
        unless ($ftype->is_active) {
            $err = "Flag $name is not active in this database\n";
            $err .= "   Dropping flag $name\n";
            return $err;
        }

        $dbh->do("INSERT INTO flags 
                 (type_id, status, bug_id, attach_id, creation_date, 
                  setter_id, requestee_id)
                  VALUES (?, ?, ?, ?, ?, ?, ?)", undef,
            ($ftype->id, $status, $bugid, $attachid, $timestamp,
            $setter_id, $requestee_id));
    }
    else {
        $err = "Dropping unknown $type flag: $name\n";
        return $err;
    }
    return $err;
}

# Converts and returns the input data as an array.
sub _to_array {
    my $value = shift;

    $value = [$value] if !ref($value);
    return @$value;
}

###############################################################################
# XML Handlers                                                                #
###############################################################################

# This subroutine gets called only once - as soon as the <bugzilla> opening
# tag is parsed. It simply checks to see that the all important exporter
# maintainer and URL base are set.
#
#    exporter:   email address of the person moving the bugs
#    maintainer: the maintainer of the bugzilla installation
#                as set in the parameters file
#    urlbase:    The urlbase parameter of the installation
#                bugs are being moved from
#
sub init() {
    my ( $twig, $bugzilla ) = @_;
    my $root       = $twig->root;
    my $maintainer = $root->{'att'}->{'maintainer'};
    my $exporter   = $root->{'att'}->{'exporter'};
    my $urlbase    = $root->{'att'}->{'urlbase'};
    my $xmlversion = $root->{'att'}->{'version'};

    if ($xmlversion ne BUGZILLA_VERSION) {
            my $log = "Possible version conflict!\n";
            $log .= "   XML was exported from Bugzilla version $xmlversion\n";
            $log .= "   But this installation uses ";
            $log .= BUGZILLA_VERSION . "\n";
            Debug($log, OK_LEVEL);
            push(@logs, $log);
    }
    Error( "no maintainer", "REOPEN", $exporter ) unless ($maintainer);
    Error( "no exporter",   "REOPEN", $exporter ) unless ($exporter);
    Error( "invalid exporter: $exporter", "REOPEN", $exporter ) if ( !login_to_id($exporter) );
    Error( "no urlbase set", "REOPEN", $exporter ) unless ($urlbase);
}
    

# Parse attachments.
#
# This subroutine is called once for each attachment in the xml file.
# It is called as soon as the closing </attachment> tag is parsed.
# Since attachments have the potential to be very large, and
# since each attachment will be inside <bug>..</bug> tags we shove
# the attachment onto an array which will be processed by process_bug
# and then disposed of. The attachment array will then contain only
# one bugs' attachments at a time.
# The cycle will then repeat for the next <bug>
#
# The attach_id is ignored since mysql generates a new one for us.
# The submitter_id gets filled in with $exporterid.

sub process_attachment() {
    my ( $twig, $attach ) = @_;
    Debug( "Parsing attachments", DEBUG_LEVEL );
    my %attachment;

    $attachment{'date'} =
        format_time( $attach->field('date'), "%Y-%m-%d %R" ) || $timestamp;
    $attachment{'desc'}       = $attach->field('desc');
    $attachment{'ctype'}      = $attach->field('type') || "unknown/unknown";
    $attachment{'attachid'}   = $attach->field('attachid');
    $attachment{'ispatch'}    = $attach->{'att'}->{'ispatch'} || 0;
    $attachment{'isobsolete'} = $attach->{'att'}->{'isobsolete'} || 0;
    $attachment{'isprivate'}  = $attach->{'att'}->{'isprivate'} || 0;
    $attachment{'filename'}   = $attach->field('filename') || "file";
    $attachment{'attacher'}   = $attach->field('attacher');
    # Attachment data is not exported in versions 2.20 and older.
    if (defined $attach->first_child('data') &&
            defined $attach->first_child('data')->{'att'}->{'encoding'}) {
        my $encoding = $attach->first_child('data')->{'att'}->{'encoding'};
        if ($encoding =~ /base64/) {
            # decode the base64
            my $data   = $attach->field('data');
            my $output = decode_base64($data);
            $attachment{'data'} = $output;
        }
        elsif ($encoding =~ /filename/) {
            # read the attachment file
            Error("attach_path is required", undef) unless ($attach_path);
            
            my $filename = $attach->field('data');
            # Remove any leading path data from the filename
            $filename =~ s/(.*\/|.*\\)//gs;
            
            my $attach_filename = $attach_path . "/" . $filename;
            open(ATTACH_FH, "<", $attach_filename) or
                Error("cannot open $attach_filename", undef);
            $attachment{'data'} = do { local $/; <ATTACH_FH> };
            close ATTACH_FH;
        }
    }
    else {
        $attachment{'data'} = $attach->field('data');
    }

    # attachment flags
    my @aflags;
    foreach my $aflag ( $attach->children('flag') ) {
        my %aflag;
        $aflag{'name'}      = $aflag->{'att'}->{'name'};
        $aflag{'status'}    = $aflag->{'att'}->{'status'};
        $aflag{'setter'}    = $aflag->{'att'}->{'setter'};
        $aflag{'requestee'} = $aflag->{'att'}->{'requestee'};
        push @aflags, \%aflag;
    }
    $attachment{'flags'} = \@aflags if (@aflags);

    # free up the memory for use by the rest of the script
    $attach->delete;
    if ($attachment{'attachid'}) {
        push @attachments, \%attachment;
    }
    else {
        push @attachments, "err";
    }
}

# This subroutine will be called once for each <bug> in the xml file.
# It is called as soon as the closing </bug> tag is parsed.
# If this bug had any <attachment> tags, they will have been processed
# before we get to this point and their data will be in the @attachments
# array.
# As each bug is processed, it is inserted into the database and then
# purged from memory to free it up for later bugs.

sub process_bug {
    my ( $twig, $bug ) = @_;
    my $root             = $twig->root;
    my $maintainer       = $root->{'att'}->{'maintainer'};
    my $exporter_login   = $root->{'att'}->{'exporter'};
    my $exporter         = new Bugzilla::User({ name => $exporter_login });
    my $urlbase          = $root->{'att'}->{'urlbase'};
    my $url              = $urlbase . $bug_page;
    trick_taint($url);

    # We will store output information in this variable.
    my $log = "";
    if ( defined $bug->{'att'}->{'error'} ) {
        $log .= "\nError in bug " . $bug->field('bug_id') . "\@$urlbase: ";
        $log .= $bug->{'att'}->{'error'} . "\n";
        if ( $bug->{'att'}->{'error'} =~ /NotFound/ ) {
            $log .= "$exporter_login tried to move bug " . $bug->field('bug_id');
            $log .= " here, but $urlbase reports that this bug";
            $log .= " does not exist.\n";
        }
        elsif ( $bug->{'att'}->{'error'} =~ /NotPermitted/ ) {
            $log .= "$exporter_login tried to move bug " . $bug->field('bug_id');
            $log .= " here, but $urlbase reports that $exporter_login does ";
            $log .= " not have access to that bug.\n";
        }
        return;
    }
    $bugtotal++;

    # This list contains all other bug fields that we want to process.
    # If it is not in this list it will not be included.
    my %all_fields;
    foreach my $field ( 
        qw(long_desc attachment flag group), Bugzilla::Bug::fields() )
    {
        $all_fields{$field} = 1;
    }
    
    my %bug_fields;
    my $err = "";

   # Loop through all the xml tags inside a <bug> and compare them to the
   # lists of fields. If they match throw them into the hash. Otherwise
   # append it to the log, which will go into the comments when we are done.
    foreach my $bugchild ( $bug->children() ) {
        Debug( "Parsing field: " . $bugchild->name, DEBUG_LEVEL );

        # Skip the token if one is included. We don't want it included in
        # the comments, and it is not used by the importer.
        next if $bugchild->name eq 'token';

        if ( defined $all_fields{ $bugchild->name } ) {
            my @values = $bug->children_text($bugchild->name);
            if (scalar @values > 1) {
                $bug_fields{$bugchild->name} = \@values;
            }
            else {
                $bug_fields{$bugchild->name} = $values[0];
            }
        }
        else {
            $err .= "Unknown bug field \"" . $bugchild->name . "\"";
            $err .= " encountered while moving bug\n";
            $err .= "   <" . $bugchild->name . ">";
            if ( $bugchild->children_count > 1 ) {
                $err .= "\n";
                foreach my $subchild ( $bugchild->children() ) {
                    $err .= "     <" . $subchild->name . ">";
                    $err .= $subchild->field;
                    $err .= "</" . $subchild->name . ">\n";
                }
            }
            else {
                $err .= $bugchild->field;
            }
            $err .= "</" . $bugchild->name . ">\n";
        }
    }

    # Parse long descriptions
    my @long_descs;
    foreach my $comment ( $bug->children('long_desc') ) {
        Debug( "Parsing Long Description", DEBUG_LEVEL );
        my %long_desc = ( who       => $comment->field('who'),
                          bug_when  => format_time($comment->field('bug_when'), '%Y-%m-%d %T'),
                          isprivate => $comment->{'att'}->{'isprivate'} || 0 );

        # If the exporter is not in the insidergroup, keep the comment public.
        $long_desc{isprivate} = 0 unless $exporter->is_insider;

        my $data = $comment->field('thetext');
        if ( defined $comment->first_child('thetext')->{'att'}->{'encoding'}
            && $comment->first_child('thetext')->{'att'}->{'encoding'} =~
            /base64/ )
        {
            $data = decode_base64($data);
        }

        # For backwards-compatibility with Bugzillas before 3.6:
        #
        # If we leave the attachment ID in the comment it will be made a link
        # to the wrong attachment. Since the new attachment ID is unknown yet
        # let's strip it out for now. We will make a comment with the right ID
        # later
        $data =~ s/Created an attachment \(id=\d+\)/Created an attachment/g;

        # Same goes for bug #'s Since we don't know if the referenced bug
        # is also being moved, lets make sure they know it means a different
        # bugzilla.
        $data =~ s/([Bb]ugs?\s*\#?\s*(\d+))/$url$2/g;

        # Keep the original commenter if possible, else we will fall back
        # to the exporter account.
        $long_desc{whoid} = login_to_id($long_desc{who});

        if (!$long_desc{whoid}) {
            $data = "The original author of this comment is $long_desc{who}.\n\n" . $data;
        }

        $long_desc{'thetext'} = $data;
        push @long_descs, \%long_desc;
    }

    my @sorted_descs = sort { $a->{'bug_when'} cmp $b->{'bug_when'} } @long_descs;

    my $comments = "\n\n--- Bug imported by $exporter_login ";
    $comments .= format_time(scalar localtime(time()), '%Y-%m-%d %R %Z') . " ";
    $comments .= " ---\n\n";
    $comments .= "This bug was previously known as _bug_ $bug_fields{'bug_id'} at ";
    $comments .= $url . $bug_fields{'bug_id'} . "\n";
    if ( defined $bug_fields{'dependson'} ) {
        $comments .= "This bug depended on bug(s) " .
                     join(' ', _to_array($bug_fields{'dependson'})) . ".\n";
    }
    if ( defined $bug_fields{'blocked'} ) {
        $comments .= "This bug blocked bug(s) " .
                     join(' ', _to_array($bug_fields{'blocked'})) . ".\n";
    }

    # Now we process each of the fields in turn and make sure they contain
    # valid data. We will create two parallel arrays, one for the query
    # and one for the values. For every field we need to push an entry onto
    # each array.
    my @query  = ();
    my @values = ();

    # Each of these fields we will check for newlines and shove onto the array
    foreach my $field (qw(status_whiteboard bug_file_loc short_desc)) {
        if ($bug_fields{$field}) {
            $bug_fields{$field} = clean_text( $bug_fields{$field} );
            push( @query,  $field );
            push( @values, $bug_fields{$field} );
        }
    }

    # Alias
    if ( $bug_fields{'alias'} ) {
        my ($alias) = $dbh->selectrow_array("SELECT COUNT(*) FROM bugs 
                                                WHERE alias = ?", undef,
                                                $bug_fields{'alias'} );
        if ($alias) {
            $err .= "Dropping conflicting bug alias ";
            $err .= $bug_fields{'alias'} . "\n";
        }
        else {
            $alias = $bug_fields{'alias'};
            push @query,  'alias';
            push @values, $alias;
        }
    }

    # Timestamps
    push( @query, "creation_ts" );
    push( @values,
        format_time( $bug_fields{'creation_ts'}, "%Y-%m-%d %T" )
          || $timestamp );

    push( @query, "delta_ts" );
    push( @values,
        format_time( $bug_fields{'delta_ts'}, "%Y-%m-%d %T" )
          || $timestamp );

    # Bug Access
    push( @query,  "cclist_accessible" );
    push( @values, $bug_fields{'cclist_accessible'} ? 1 : 0 );

    push( @query,  "reporter_accessible" );
    push( @values, $bug_fields{'reporter_accessible'} ? 1 : 0 );

    my $product = new Bugzilla::Product(
        { name => $bug_fields{'product'} || '' });
    if (!$product) {
        $err .= "Unknown Product " . $bug_fields{'product'} . "\n";
        $err .= "   Using default product set at the command line.\n";
        $product = new Bugzilla::Product({ name => $default_product_name })
            or Error("an invalid default product was defined for the target"
                     . " DB. " . $params->{"maintainer"} . " needs to specify "
                     . "--product when calling importxml.pl", "REOPEN", 
                     $exporter);
    }
    my $component = new Bugzilla::Component({
        product => $product, name => $bug_fields{'component'} || '' });
    if (!$component) {
        $err .= "Unknown Component " . $bug_fields{'component'} . "\n";
        $err .= "   Using default product and component set ";
        $err .= "at the command line.\n";

        $product = new Bugzilla::Product({ name => $default_product_name });
        $component = new Bugzilla::Component({
           name => $default_component_name, product => $product });
        if (!$component) {
            Error("an invalid default component was defined for the target" 
                  . " DB. ".  $params->{"maintainer"} . " needs to specify " 
                  . "--component when calling importxml.pl", "REOPEN", 
                  $exporter);
        }
    }

    my $prod_id = $product->id;
    my $comp_id = $component->id;

    push( @query,  "product_id" );
    push( @values, $prod_id );
    push( @query,  "component_id" );
    push( @values, $comp_id );

    # Since there is no default version for a product, we check that the one
    # coming over is valid. If not we will use the first one in @versions
    # and warn them.
    my $version = new Bugzilla::Version(
          { product => $product, name => $bug_fields{'version'} });

    push( @query, "version" );
    if ($version) {
        push( @values, $version->name );
    }
    else {
        my @versions = @{ $product->versions };
        my $v        = $versions[0];
        push( @values, $v->name );
        $err .= "Unknown version \"";
        $err .= ( defined $bug_fields{'version'} )
            ? $bug_fields{'version'}
            : "unknown";
        $err .= " in product " . $product->name . ". \n";
        $err .= "   Setting version to \"" . $v->name . "\".\n";
    }

    # Milestone
    if ( $params->{"usetargetmilestone"} ) {
        my $milestone;
        if (defined $bug_fields{'target_milestone'}
            && $bug_fields{'target_milestone'} ne "") {

            $milestone = new Bugzilla::Milestone(
                { product => $product, name => $bug_fields{'target_milestone'} });
        }
        if ($milestone) {
            push( @values, $milestone->name );
        }
        else {
            push( @values, $product->default_milestone );
            $err .= "Unknown milestone \"";
            $err .= ( defined $bug_fields{'target_milestone'} )
                ? $bug_fields{'target_milestone'}
                : "unknown";
            $err .= " in product " . $product->name . ". \n";
            $err .= "   Setting to default milestone for this product, ";
            $err .= "\"" . $product->default_milestone . "\".\n";
        }
        push( @query, "target_milestone" );
    }

    # For priority, severity, opsys and platform we check that the one being
    # imported is valid. If it is not we use the defaults set in the parameters.
    if (defined( $bug_fields{'bug_severity'} )
        && check_field('bug_severity', scalar $bug_fields{'bug_severity'},
                       undef, ERR_LEVEL) )
    {
        push( @values, $bug_fields{'bug_severity'} );
    }
    else {
        push( @values, $params->{'defaultseverity'} );
        $err .= "Unknown severity ";
        $err .= ( defined $bug_fields{'bug_severity'} )
          ? $bug_fields{'bug_severity'}
          : "unknown";
        $err .= ". Setting to default severity \"";
        $err .= $params->{'defaultseverity'} . "\".\n";
    }
    push( @query, "bug_severity" );

    if (defined( $bug_fields{'priority'} )
        && check_field('priority', scalar $bug_fields{'priority'},
                       undef, ERR_LEVEL ) )
    {
        push( @values, $bug_fields{'priority'} );
    }
    else {
        push( @values, $params->{'defaultpriority'} );
        $err .= "Unknown priority ";
        $err .= ( defined $bug_fields{'priority'} )
          ? $bug_fields{'priority'}
          : "unknown";
        $err .= ". Setting to default priority \"";
        $err .= $params->{'defaultpriority'} . "\".\n";
    }
    push( @query, "priority" );

    if (defined( $bug_fields{'rep_platform'} )
        && check_field('rep_platform', scalar $bug_fields{'rep_platform'},
                       undef, ERR_LEVEL ) )
    {
        push( @values, $bug_fields{'rep_platform'} );
    }
    else {
        push( @values, $params->{'defaultplatform'} );
        $err .= "Unknown platform ";
        $err .= ( defined $bug_fields{'rep_platform'} )
          ? $bug_fields{'rep_platform'}
          : "unknown";
        $err .=". Setting to default platform \"";
        $err .= $params->{'defaultplatform'} . "\".\n";
    }
    push( @query, "rep_platform" );

    if (defined( $bug_fields{'op_sys'} )
        && check_field('op_sys',  scalar $bug_fields{'op_sys'},
                       undef, ERR_LEVEL ) )
    {
        push( @values, $bug_fields{'op_sys'} );
    }
    else {
        push( @values, $params->{'defaultopsys'} );
        $err .= "Unknown operating system ";
        $err .= ( defined $bug_fields{'op_sys'} )
          ? $bug_fields{'op_sys'}
          : "unknown";
        $err .= ". Setting to default OS \"" . $params->{'defaultopsys'} . "\".\n";
    }
    push( @query, "op_sys" );

    # Process time fields
    if ( $params->{"timetrackinggroup"} ) {
        my $date = validate_date( $bug_fields{'deadline'} ) ? $bug_fields{'deadline'} : undef;
        push( @values, $date );
        push( @query,  "deadline" );
        if ( defined $bug_fields{'estimated_time'} ) {
            eval {
                Bugzilla::Object::_validate_time($bug_fields{'estimated_time'}, "e");
            };
            if (!$@){
                push( @values, $bug_fields{'estimated_time'} );
                push( @query,  "estimated_time" );
            }
        }
        if ( defined $bug_fields{'remaining_time'} ) {
            eval {
                Bugzilla::Object::_validate_time($bug_fields{'remaining_time'}, "r");
            };
            if (!$@){
                push( @values, $bug_fields{'remaining_time'} );
                push( @query,  "remaining_time" );
            }
        }
        if ( defined $bug_fields{'actual_time'} ) {
            eval {
                Bugzilla::Object::_validate_time($bug_fields{'actual_time'}, "a");
            };
            if ($@){
                $bug_fields{'actual_time'} = 0.0;
                $err .= "Invalid Actual Time. Setting to 0.0\n";
            }
        }
        else {
            $bug_fields{'actual_time'} = 0.0;
            $err .= "Actual time not defined. Setting to 0.0\n";
        }
    }

    # Reporter Assignee QA Contact
    my $exporterid = $exporter->id;
    my $reporterid = login_to_id( $bug_fields{'reporter'} )
      if $bug_fields{'reporter'};
    push( @query, "reporter" );
    if ( ( $bug_fields{'reporter'} ) && ($reporterid) ) {
        push( @values, $reporterid );
    }
    else {
        push( @values, $exporterid );
        $err .= "The original reporter of this bug does not have\n";
        $err .= "   an account here. Reassigning to the person who moved\n";
        $err .= "   it here: $exporter_login.\n";
        if ( $bug_fields{'reporter'} ) {
            $err .= "   Previous reporter was $bug_fields{'reporter'}.\n";
        }
        else {
            $err .= "   Previous reporter is unknown.\n";
        }
    }

    my $changed_owner = 0;
    my $owner;
    push( @query, "assigned_to" );
    if ( ( $bug_fields{'assigned_to'} )
        && ( $owner = login_to_id( $bug_fields{'assigned_to'} )) ) {
        push( @values, $owner );
    }
    else {
        push( @values, $component->default_assignee->id );
        $changed_owner = 1;
        $err .= "The original assignee of this bug does not have\n";
        $err .= "   an account here. Reassigning to the default assignee\n";
        $err .= "   for the component, ". $component->default_assignee->login .".\n";
        if ( $bug_fields{'assigned_to'} ) {
            $err .= "   Previous assignee was $bug_fields{'assigned_to'}.\n";
        }
        else {
            $err .= "   Previous assignee is unknown.\n";
        }
    }

    if ( $params->{"useqacontact"} ) {
        my $qa_contact;
        push( @query, "qa_contact" );
        if ( ( defined $bug_fields{'qa_contact'})
            && ( $qa_contact = login_to_id( $bug_fields{'qa_contact'} ) ) ) {
            push( @values, $qa_contact );
        }
        else {
            push(@values, $component->default_qa_contact ?
                          $component->default_qa_contact->id : undef);

            if ($component->default_qa_contact) {
                $err .= "Setting qa contact to the default for this product.\n";
                $err .= "   This bug either had no qa contact or an invalid one.\n";
            }
        }
    }

    # Status & Resolution
    my $valid_res = check_field('resolution',  
                                  scalar $bug_fields{'resolution'}, 
                                  undef, ERR_LEVEL );
    my $valid_status = check_field('bug_status',  
                                  scalar $bug_fields{'bug_status'}, 
                                  undef, ERR_LEVEL );
    my $status = $bug_fields{'bug_status'} || undef;
    my $resolution = $bug_fields{'resolution'} || undef;
    
    # Check everconfirmed 
    my $everconfirmed;
    if ($product->allows_unconfirmed) {
        $everconfirmed = $bug_fields{'everconfirmed'} || 0;
    }
    else {
        $everconfirmed = 1;
    }
    push (@query,  "everconfirmed");
    push (@values, $everconfirmed);

    # Sanity check will complain about having bugs marked duplicate but no
    # entry in the dup table. Since we can't tell the bug ID of bugs
    # that might not yet be in the database we have no way of populating
    # this table. Change the resolution instead.
    if ( $valid_res  && ( $bug_fields{'resolution'} eq "DUPLICATE" ) ) {
        $resolution = "INVALID";
        $err .= "This bug was marked DUPLICATE in the database ";
        $err .= "it was moved from.\n    Changing resolution to \"INVALID\"\n";
    } 

    # If there is at least 1 initial bug status different from UNCO, use it,
    # else use the open bug status with the lowest sortkey (different from UNCO).
    my @bug_statuses = @{Bugzilla::Status->can_change_to()};
    @bug_statuses = grep { $_->name ne 'UNCONFIRMED' } @bug_statuses;

    my $initial_status;
    if (scalar(@bug_statuses)) {
        $initial_status = $bug_statuses[0]->name;
    }
    else {
        @bug_statuses = Bugzilla::Status->get_all();
        # Exclude UNCO and inactive bug statuses.
        @bug_statuses = grep { $_->is_active && $_->name ne 'UNCONFIRMED'} @bug_statuses;
        my @open_statuses = grep { $_->is_open } @bug_statuses;
        if (scalar(@open_statuses)) {
            $initial_status = $open_statuses[0]->name;
        }
        else {
            # There is NO other open bug statuses outside UNCO???
            Error("no open bug statuses available.");
        }
    }

    if ($status) {
        if($valid_status){
            if (is_open_state($status)) {
                if ($resolution) {
                    $err .= "Resolution set on an open status.\n";
                    $err .= "   Dropping resolution $resolution\n";
                    $resolution = undef;
                }
                if($changed_owner){
                    if($everconfirmed){  
                        $status = $initial_status;
                    }
                    else{
                        $status = "UNCONFIRMED";
                    }
                    if ($status ne $bug_fields{'bug_status'}){
                        $err .= "Bug reassigned, setting status to \"$status\".\n";
                        $err .= "   Previous status was \"";
                        $err .=  $bug_fields{'bug_status'} . "\".\n";
                    }
                }
                if($everconfirmed){
                    if($status eq "UNCONFIRMED"){
                        $err .= "Bug Status was UNCONFIRMED but everconfirmed was true\n";
                        $err .= "   Setting status to $initial_status\n";
                        $status = $initial_status;
                    }
                }
                else{ # $everconfirmed is false
                    if($status ne "UNCONFIRMED"){
                        $err .= "Bug Status was $status but everconfirmed was false\n";
                        $err .= "   Setting status to UNCONFIRMED\n";
                        $status = "UNCONFIRMED";
                    }
                }
            }
            else {
               if (!$resolution) {
                   $err .= "Missing Resolution. Setting status to ";
                   if($everconfirmed){
                       $status = $initial_status;
                       $err .= "$initial_status\n";
                   }
                   else{
                       $status = "UNCONFIRMED";
                       $err .= "UNCONFIRMED\n";
                   }
               }
               elsif (!$valid_res) {
                   $err .= "Unknown resolution \"$resolution\".\n";
                   $err .= "   Setting resolution to INVALID\n";
                   $resolution = "INVALID";
               }
            }   
        }
        else{ # $valid_status is false
            if($everconfirmed){  
                $status = $initial_status;
            }
            else{
                $status = "UNCONFIRMED";
            }        
            $err .= "Bug has invalid status, setting status to \"$status\".\n";
            $err .= "   Previous status was \"";
            $err .=  $bug_fields{'bug_status'} . "\".\n";
            $resolution = undef;
        }
    }
    else {
        if($everconfirmed){  
            $status = $initial_status;
        }
        else{
            $status = "UNCONFIRMED";
        }        
        $err .= "Bug has no status, setting status to \"$status\".\n";
        $err .= "   Previous status was unknown\n";
        $resolution = undef;
    }

    if ($resolution) {
        push( @query,  "resolution" );
        push( @values, $resolution );
    }
    
    # Bug status
    push( @query,  "bug_status" );
    push( @values, $status );

    # Custom fields - Multi-select fields have their own table.
    my %multi_select_fields;
    foreach my $field (Bugzilla->active_custom_fields) {
        my $custom_field = $field->name;
        my $value = $bug_fields{$custom_field};
        next unless defined $value;
        if ($field->type == FIELD_TYPE_FREETEXT) {
            push(@query, $custom_field);
            push(@values, clean_text($value));
        } elsif ($field->type == FIELD_TYPE_TEXTAREA) {
            push(@query, $custom_field);
            push(@values, $value);
        } elsif ($field->type == FIELD_TYPE_SINGLE_SELECT) {
            my $is_well_formed = check_field($custom_field, $value, undef, ERR_LEVEL);
            if ($is_well_formed) {
                push(@query, $custom_field);
                push(@values, $value);
            } else {
                $err .= "Skipping illegal value \"$value\" in $custom_field.\n" ;
            }
        } elsif ($field->type == FIELD_TYPE_MULTI_SELECT) {
            my @legal_values;
            foreach my $item (_to_array($value)) {
                my $is_well_formed = check_field($custom_field, $item, undef, ERR_LEVEL);
                if ($is_well_formed) {
                    push(@legal_values, $item);
                } else {
                    $err .= "Skipping illegal value \"$item\" in $custom_field.\n" ;
                }
            }
            if (scalar @legal_values) {
                $multi_select_fields{$custom_field} = \@legal_values;
            }
        } elsif ($field->type == FIELD_TYPE_DATETIME) {
            eval { $value = Bugzilla::Bug->_check_datetime_field($value); };
            if ($@) {
                $err .= "Skipping illegal value \"$value\" in $custom_field.\n" ;
            }
            else {
                push(@query, $custom_field);
                push(@values, $value);
            }
        } elsif ($field->type == FIELD_TYPE_DATE) {
            eval { $value = Bugzilla::Bug->_check_date_field($value); };
            if ($@) {
                $err .= "Skipping illegal value \"$value\" in $custom_field.\n" ;
            }
            else {
                push(@query, $custom_field);
                push(@values, $value);
            }
        } else {
            $err .= "Type of custom field $custom_field is an unhandled FIELD_TYPE: " .
                    $field->type . "\n";
        }
    }

    # For the sake of sanitycheck.cgi we do this.
    # Update lastdiffed if you do not want to have mail sent
    unless ($mail) {
        push @query,  "lastdiffed";
        push @values, $timestamp;
    }

    # INSERT the bug
    my $query = "INSERT INTO bugs (" . join( ", ", @query ) . ") VALUES (";
       $query .= '?,' foreach (@values);
    chop($query);    # Remove the last comma.
       $query .= ")";

    $dbh->do( $query, undef, @values );
    my $id = $dbh->bz_last_key( 'bugs', 'bug_id' );
    my $bug_obj = Bugzilla::Bug->new($id);

    # We are almost certain to get some uninitialized warnings
    # Since this is just for debugging the query, let's shut them up
    eval {
        no warnings 'uninitialized';
        Debug(
            "Bug Query: INSERT INTO bugs (\n"
              . join( ",\n", @query )
              . "\n) VALUES (\n"
              . join( ",\n", @values ),
            DEBUG_LEVEL
        );
    };

    # Handle CC's
    if ( defined $bug_fields{'cc'} ) {
        my %ccseen;
        my $sth_cc = $dbh->prepare("INSERT INTO cc (bug_id, who) VALUES (?,?)");
        foreach my $person (_to_array($bug_fields{'cc'})) {
            next unless $person;
            my $uid;
            if ($uid = login_to_id($person)) {
                if ( !$ccseen{$uid} ) {
                    $sth_cc->execute( $id, $uid );
                    $ccseen{$uid} = 1;
                }
            }
            else {
                $err .= "CC member $person does not have an account here\n";
            }
        }
    }

    # Handle keywords
    if ( defined( $bug_fields{'keywords'} ) ) {
        my %keywordseen;
        my $key_sth = $dbh->prepare(
            "INSERT INTO keywords 
                      (bug_id, keywordid) VALUES (?,?)"
        );
        foreach my $keyword ( split( /[\s,]+/, $bug_fields{'keywords'} )) {
            next unless $keyword;
            my $keyword_obj = new Bugzilla::Keyword({name => $keyword});
            if (!$keyword_obj) {
                $err .= "Skipping unknown keyword: $keyword.\n";
                next;
            }
            if (!$keywordseen{$keyword_obj->id}) {
                $key_sth->execute($id, $keyword_obj->id);
                $keywordseen{$keyword_obj->id} = 1;
            }
        }
    }

    # Insert values of custom multi-select fields. They have already
    # been validated.
    foreach my $custom_field (keys %multi_select_fields) {
        my $sth = $dbh->prepare("INSERT INTO bug_$custom_field
                                 (bug_id, value) VALUES (?, ?)");
        foreach my $value (@{$multi_select_fields{$custom_field}}) {
            $sth->execute($id, $value);
        }
    }

    # Parse bug flags
    foreach my $bflag ( $bug->children('flag')) {
        next unless ( defined($bflag) );
        $err .= flag_handler(
            $bflag->{'att'}->{'name'},   $bflag->{'att'}->{'status'},
            $bflag->{'att'}->{'setter'}, $bflag->{'att'}->{'requestee'},
            $exporterid,                 $id,
            $comp_id,                    $prod_id,
            undef
        );
    }

    # Insert Attachments for the bug
    foreach my $att (@attachments) {
        if ($att eq "err"){
            $err .= "No attachment ID specified, dropping attachment\n";
            next;
        }

        my $attacher;
        if ($att->{'attacher'}) {
            $attacher = Bugzilla::User->new({name => $att->{'attacher'}, cache => 1});
        }
        my $new_attacher = $attacher || $exporter;

        if ($att->{'isprivate'} && !$new_attacher->is_insider) {
            my $who = $new_attacher->login;
            $err .= "$who not in insidergroup and attachment marked private.\n";
            $err .= "   Marking attachment public\n";
            $att->{'isprivate'} = 0;
        }

        # We log in the user so that the attachment creator is set correctly.
        Bugzilla->set_user($new_attacher);

        my $attachment = Bugzilla::Attachment->create(
            { bug           => $bug_obj,
              creation_ts   => $att->{date},
              data          => $att->{data},
              description   => $att->{desc},
              filename      => $att->{filename},
              ispatch       => $att->{ispatch},
              isprivate     => $att->{isprivate},
              isobsolete    => $att->{isobsolete},
              mimetype      => $att->{ctype},
            });
        my $att_id = $attachment->id;

        # We log out the attacher as the remaining steps are not on his behalf.
        Bugzilla->logout_request;

        $comments .= "Imported an attachment (id=$att_id)\n";
        if (!$attacher) {
            if ($att->{'attacher'}) {
                $err .= "The original submitter of attachment $att_id was\n   ";
                $err .= $att->{'attacher'} . ", but they don't have an account here.\n";
            }
            else {
                $err .= "The original submitter of attachment $att_id is unknown.\n";
            }
            $err .= "   Reassigning to the person who moved it here: $exporter_login.\n";
        }

        # Process attachment flags
        foreach my $aflag (@{ $att->{'flags'} }) {
            next unless defined($aflag) ;
            $err .= flag_handler(
                $aflag->{'name'},   $aflag->{'status'},
                $aflag->{'setter'}, $aflag->{'requestee'},
                $exporterid,        $id,
                $comp_id,           $prod_id,
                $att_id
            );
        }
    }

    # Clear the attachments array for the next bug
    @attachments = ();

    # Insert comments and append any errors
    my $worktime = $bug_fields{'actual_time'} || 0.0;
    $worktime = 0.0 if (!$exporter->is_timetracker);
    $comments .= "\n$err\n" if $err;

    my $sth_comment =
      $dbh->prepare('INSERT INTO longdescs (bug_id, who, bug_when, isprivate,
                                            thetext, work_time)
                     VALUES (?, ?, ?, ?, ?, ?)');

    foreach my $c (@sorted_descs) {
        $sth_comment->execute($id, $c->{whoid} || $exporterid, $c->{bug_when},
                              $c->{isprivate}, $c->{thetext}, 0);
    }
    $sth_comment->execute($id, $exporterid, $timestamp, 0, $comments, $worktime);
    Bugzilla::Bug->new($id)->_sync_fulltext( new_bug => 1);

    # Add this bug to each group of which its product is a member.
    my $sth_group = $dbh->prepare("INSERT INTO bug_group_map (bug_id, group_id) 
                         VALUES (?, ?)");
    foreach my $group_id ( keys %{ $product->group_controls } ) {
        if ($product->group_controls->{$group_id}->{'membercontrol'} != CONTROLMAPNA
            && $product->group_controls->{$group_id}->{'othercontrol'} != CONTROLMAPNA){
            $sth_group->execute( $id, $group_id );
        }
    }

    $log .= "Bug ${url}$bug_fields{'bug_id'} ";
    $log .= "imported as bug $id.\n";
    $log .= $params->{"urlbase"} . "show_bug.cgi?id=$id\n\n";
    if ($err) {
        $log .= "The following problems were encountered while creating bug $id.\n";
        $log .= $err;
        $log .= "You may have to set certain fields in the new bug by hand.\n\n";
    }
    Debug( $log, OK_LEVEL );
    push(@logs, $log);
    Bugzilla::BugMail::Send( $id, { 'changer' => $exporter } ) if ($mail);

    # done with the xml data. Lets clear it from memory
    $twig->purge;

}

Debug( "Reading xml", DEBUG_LEVEL );

# Read STDIN in slurp mode. VERY dangerous, but we live on the wild side ;-)
local ($/);
$xml = <>;

# If there's anything except whitespace before <?xml then we guess it's a mail
# and MIME::Parser should parse it. Else don't.
if ($xml =~ m/\S.*<\?xml/s ) {

    # If the email was encoded (Mailer::MessageToMTA() does it when using UTF-8),
    # we have to decode it first, else the XML parsing will fail.
    my $parser = MIME::Parser->new;
    $parser->output_to_core(1);
    $parser->tmp_to_core(1);
    my $entity = $parser->parse_data($xml);
    my $bodyhandle = $entity->bodyhandle;
    $xml = $bodyhandle->as_string;

}

# remove everything in file before xml header
$xml =~ s/^.+(<\?xml version.+)$/$1/s;

Debug( "Parsing tree", DEBUG_LEVEL );
my $twig = XML::Twig->new(
    twig_handlers => {
        bug        => \&process_bug,
        attachment => \&process_attachment
    },
    start_tag_handlers => { bugzilla => \&init }
);
# Prevent DoS using the billion laughs attack.
$twig->{NoExpand} = 1;

$twig->parse($xml);
my $root       = $twig->root;
my $maintainer = $root->{'att'}->{'maintainer'};
my $exporter   = $root->{'att'}->{'exporter'};
my $urlbase    = $root->{'att'}->{'urlbase'};

# It is time to email the result of the import.
my $log = join("\n\n", @logs);
$log .=  "\n\nImported $bugtotal bug(s) from $urlbase,\n  sent by $exporter.\n";
my $subject =  "$bugtotal Bug(s) successfully moved from $urlbase to " 
   . $params->{"urlbase"};
my @to = ($exporter, $maintainer);
MailMessage( $subject, $log, @to );

__END__

=head1 NAME

importxml - Import bugzilla bug data from xml.

=head1 SYNOPSIS

 importxml.pl [options] [file ...]

=head1 OPTIONS

=over

=item B<-?>

Print a brief help message and exit.

=item B<-v>

Print error and debug information. Mulltiple -v increases verbosity

=item B<-m> B<--sendmail>

Send mail to exporter with a log of bugs imported and any errors.

=item B<--attach_path>

The path to the attachment files. (Required if encoding="filename"
is used for attachments.)

=item B<--bug_page>

The page that links to the bug on top of urlbase. Its default value
is "show_bug.cgi?id=", which is what Bugzilla installations use.
You only need to pass this argument if you are importing bugs from
another bug tracking system.

=item B<--product=name>

The product to put the bug in if the product specified in the
XML doesn't exist.

=item B<--component=name>

The component to put the bug in if the component specified in the
XML doesn't exist.

=back

=head1 DESCRIPTION

     This script is used to import bugs from another installation of bugzilla.
     It can be used in two ways.
     First using the move function of bugzilla
     on another system will send mail to an alias provided by
     the administrator of the target installation (you). Set up an alias
     similar to the one given below so this mail will be automatically 
     run by this script and imported into your database.  Run 'newaliases'
     after adding this alias to your aliases file. Make sure your sendmail
     installation is configured to allow mail aliases to execute code. 

     bugzilla-import: "|/usr/bin/perl /opt/bugzilla/importxml.pl --mail"

     Second it can be run from the command line with any xml file from 
     STDIN that conforms to the bugzilla DTD. In this case you can pass 
     an argument to set whether you want to send the
     mail that will be sent to the exporter and maintainer normally.

     importxml.pl [options] bugsfile.xml

=cut

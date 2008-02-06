#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.

# The purpose of this script is to take an email message, which
# specifies a bugid and append it to the bug as part of the longdesc
# table

# Contributor : Seth M. Landsman <seth@dworkin.net>

# 03/15/00 : Initial version by SML
# 03/15/00 : processmail gets called

# Email subject must be of format :
# .* Bug ### .*
# replying to a typical bugzilla email should be valid

# TODO : 
# 1. better way to get the body text (I don't know what dump_entity() is 
#   actually doing

use strict;
use MIME::Parser;

BEGIN {
  chdir "..";        # this script lives in contrib, change to main
  push @INC, "contrib";
  push @INC, "."; # this script lives in contrib
}

require "globals.pl";
use BugzillaEmail;
use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::BugMail;

my $dbh = Bugzilla->dbh;

# Create a new MIME parser:
my $parser = new MIME::Parser;

my $Comment = "";

# Create and set the output directory:
# FIXME: There should be a $BUGZILLA_HOME variable (SML)
(-d "$datadir/mimedump-tmp") or mkdir "$datadir/mimedump-tmp",0755 or die "mkdir: $!";
(-w "$datadir/mimedump-tmp") or die "can't write to directory";

$parser->output_dir("$datadir/mimedump-tmp");
    
# Read the MIME message:
my $entity = $parser->read(\*STDIN) or die "couldn't parse MIME stream";
$entity->remove_sig(10);          # Removes the signature in the last 10 lines

# Getting values from parsed mail
my $Sender = $entity->get( 'From' );
$Sender ||=  $entity->get( 'Reply-To' );
my $Message_ID = $entity->get( 'Message-Id' );

die (" *** Cant find Sender-adress in sent mail ! ***\n" ) unless defined( $Sender );
chomp( $Sender );
chomp( $Message_ID );

print "Dealing with the sender $Sender\n";

my $SenderShort = $Sender;
$SenderShort =~ s/^.*?([a-zA-Z0-9_.-]+?\@[a-zA-Z0-9_.-]+\.[a-zA-Z0-9_.-]+).*$/$1/;

$SenderShort = findUser($SenderShort);

print "SenderShort is $SenderShort\n";
if (!defined($SenderShort)) {
  $SenderShort = $Sender;
  $SenderShort =~ s/^.*?([a-zA-Z0-9_.-]+?\@[a-zA-Z0-9_.-]+\.[a-zA-Z0-9_.-]+).*$/$1/;
}
print "The sendershort is now $SenderShort\n";

if (!defined($SenderShort)) {
  DealWithError("No such user $SenderShort exists.");
}

my $Subject = $entity->get('Subject');
print "The subject is $Subject\n";

my ($bugid) = ($Subject =~ /\[Bug ([\d]+)\]/);
print "The bugid is $bugid\n";

# make sure the bug exists

SendSQL("SELECT bug_id FROM bugs WHERE bug_id = $bugid;");
my $found_id = FetchOneColumn();
print "Did we find the bug? $found_id-\n";
if (!defined($found_id)) {
  DealWithError("Bug $bugid does not exist");
}

# get the user id
SendSQL("SELECT userid FROM profiles WHERE " . 
        $dbh->sql_istrcmp('login_name', $dbh->quote($SenderShort)));
my $userid = FetchOneColumn();
if (!defined($userid)) {
  DealWithError("Userid not found for $SenderShort");
}

# parse out the text of the message
dump_entity($entity);

# Get rid of the bug id
$Subject =~ s/\[Bug [\d]+\]//;
#my $Comment = "This is only a test ...";
my $Body = "Subject: " . $Subject . "\n" . $Comment;

# shove it in the table
my $long_desc_query = "INSERT INTO longdescs SET bug_id=$found_id, who=$userid, bug_when=NOW(), thetext=" . SqlQuote($Body) . ";";
SendSQL($long_desc_query);

Bugzilla::BugMail::Send( $found_id, { changer => $SenderShort } );

sub DealWithError {
  my ($reason) = @_;
  print $reason . "\n";
  exit 100;
}

# Yanking this wholesale from bug_email, 'cause I know this works.  I'll
#  figure out what it really does later
#------------------------------
#
# dump_entity ENTITY, NAME
#
# Recursive routine for parsing a mime coded mail.
# One mail may contain more than one mime blocks, which need to be
# handled. Therefore, this function is called recursively.
#
# It gets the for bugzilla important information from the mailbody and 
# stores them into the global attachment-list @attachments. The attachment-list
# is needed in storeAttachments.
#
sub dump_entity {
    my ($entity, $name) = @_;
    defined($name) or $name = "'anonymous'";
    my $IO;


    # Output the body:
    my @parts = $entity->parts;
    if (@parts) {                     # multipart...
	my $i;
	foreach $i (0 .. $#parts) {       # dump each part...
	    dump_entity($parts[$i], ("$name, part ".(1+$i)));
	}
    } else {                            # single part...	

	# Get MIME type, and display accordingly...
	my $msg_part = $entity->head->get( 'Content-Disposition' );
	
	$msg_part ||= ""; 

	my ($type, $subtype) = split('/', $entity->head->mime_type);
	my $body = $entity->bodyhandle;
	my ($data, $on_disk );

	if(  $msg_part =~ /^attachment/ ) {
	    # Attached File
	    my $des = $entity->head->get('Content-Description');
	    $des ||= "";

	    if( defined( $body->path )) { # Data is on disk
		$on_disk = 1;
		$data = $body->path;
		
	    } else {                      # Data is in core
		$on_disk = 0;
		$data = $body->as_string;
	    }
#	    push ( @attachments, [ $data, $entity->head->mime_type, $on_disk, $des ] );
	} else {
	    # Real Message
	    if ($type =~ /^(text|message)$/) {     # text: display it...
		if ($IO = $body->open("r")) {
		    $Comment .=  $_ while (defined($_ = $IO->getline));
		    $IO->close;
		} else {       # d'oh!
		    print "$0: couldn't find/open '$name': $!";
		}
	    } else { print "Oooops - no Body !\n"; }
	}
    }
}

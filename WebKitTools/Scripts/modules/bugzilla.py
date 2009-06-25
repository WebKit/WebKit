# Copyright (c) 2009, Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# WebKit's Python module for interacting with Bugzilla

import getpass
import subprocess
import sys
import urllib2

try:
    from BeautifulSoup import BeautifulSoup
    from mechanize import Browser
except ImportError, e:
    print """
BeautifulSoup and mechanize are required.

To install:
sudo easy_install BeautifulSoup mechanize

Or from the web:
http://www.crummy.com/software/BeautifulSoup/
http://wwwsearch.sourceforge.net/mechanize/
"""
    exit(1)

def log(string):
    print >> sys.stderr, string

# FIXME: This should not depend on git for config storage
def read_config(key):
    # Need a way to read from svn too
    config_process = subprocess.Popen("git config --get bugzilla." + key, stdout=subprocess.PIPE, shell=True)
    value = config_process.communicate()[0]
    return_code = config_process.wait()

    if return_code:
        return None
    return value.rstrip('\n')

class Bugzilla:
    def __init__(self, dryrun=False):
        self.dryrun = dryrun
        self.authenticated = False
        
        # Defaults (until we support better option parsing):
        self.bug_server = "https://bugs.webkit.org/"
        
        self.browser = Browser()
        # Ignore bugs.webkit.org/robots.txt until we fix it to allow this script
        self.browser.set_handle_robots(False)

    # This could eventually be a text file
    reviewer_usernames_to_full_names = {
        "abarth" : "Adam Barth",
        "adele" : "Adele Peterson",
        "aroben" : "Adam Roben",
        "ap" : "Alexey Proskuryakov",
        "ariya.hidayat" : "Ariya Hidayat",
        "beidson" : "Brady Eidson",
        "darin" : "Darin Adler",
        "ddkilzer" : "David Kilzer",
        "dglazkov" : "Dimitri Glazkov",
        "eric" : "Eric Seidel",
        "fishd" : "Darin Fisher",
        "gns" : "Gustavo Noronha",
        "hyatt" : "David Hyatt",
        "jmalonzo" : "Jan Alonzo",
        "justin.garcia" : "Justin Garcia",
        "kevino" : "Kevin Ollivier",
        "levin" : "David Levin",
        "mitz" : "Dan Bernstein",
        "mjs" : "Maciej Stachowiak",
        "mrowe" : "Mark Rowe",
        "oliver" : "Oliver Hunt",
        "sam" : "Sam Weinig",
        "staikos" : "George Staikos",
        "timothy" : "Timothy Hatcher",
        "treat" : "Adam Treat",
        "vestbo" : u'Tor Arne Vestb\xf8',
        "xan.lopez" : "Xan Lopez",
        "zecke" : "Holger Freyther",
        "zimmermann" : "Nikolas Zimmermann",
    }

    def full_name_from_bugzilla_name(self, bugzilla_name):
        if not bugzilla_name in self.reviewer_usernames_to_full_names:
            raise Exception("ERROR: Unknown reviewer! " + bugzilla_name)
        return self.reviewer_usernames_to_full_names[bugzilla_name]

    def bug_url_for_bug_id(self, bug_id):
        bug_base_url = self.bug_server + "show_bug.cgi?id="
        return "%s%s" % (bug_base_url, bug_id)
    
    def attachment_url_for_id(self, attachment_id, action="view"):
        attachment_base_url = self.bug_server + "attachment.cgi?id="
        return "%s%s&action=%s" % (attachment_base_url, attachment_id, action)

    def fetch_attachments_from_bug(self, bug_id):
        bug_url = self.bug_url_for_bug_id(bug_id)
        log("Fetching: " + bug_url)

        page = urllib2.urlopen(bug_url)
        soup = BeautifulSoup(page)
    
        attachment_table = soup.find('table', {'cellspacing':"0", 'cellpadding':"4", 'border':"1"})
    
        attachments = []
        # Grab a list of non-obsoleted patch files 
        for attachment_row in attachment_table.findAll('tr'):
            first_cell = attachment_row.find('td')
            if not first_cell:
                continue # This is the header, no cells
            if first_cell.has_key('colspan'):
                break # this is the last row
            
            attachment = {}
            attachment['obsolete'] = (attachment_row.has_key('class') and attachment_row['class'] == "bz_obsolete")
            
            cells = attachment_row.findAll('td')
            attachment_link = cells[0].find('a')
            attachment['url'] = self.bug_server + attachment_link['href'] # urls are relative
            attachment['id'] = attachment['url'].split('=')[1] # e.g. https://bugs.webkit.org/attachment.cgi?id=31223
            attachment['name'] = attachment_link.string
            # attachment['type'] = cells[1]
            # attachment['date'] = cells[2]
            # attachment['size'] = cells[3]
            review_status = cells[4]
            # action_links = cells[5]

            if str(review_status).find("review+") != -1:
                reviewer = review_status.contents[0].split(':')[0] # name:\n review+\n
                reviewer_full_name = self.full_name_from_bugzilla_name(reviewer)
                attachment['reviewer'] = reviewer_full_name

            attachments.append(attachment)
        return attachments

    def fetch_reviewed_patches_from_bug(self, bug_id):
        reviewed_patches = []
        for attachment in self.fetch_attachments_from_bug(bug_id):
            if 'reviewer' in attachment and not attachment['obsolete']:
                reviewed_patches.append(attachment)
        return reviewed_patches

    def fetch_bug_ids_from_commit_queue(self):
        # FIXME: We should have an option for restricting the search by email.  Example:
        # unassigned_only = "&emailassigned_to1=1&emailtype1=substring&email1=unassigned"
        commit_queue_url = "https://bugs.webkit.org/buglist.cgi?query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&field0-0-0=flagtypes.name&type0-0-0=equals&value0-0-0=review%2B"
        log("Loading commit queue")

        page = urllib2.urlopen(commit_queue_url)
        soup = BeautifulSoup(page)
    
        bug_ids = []
        # Grab the cells in the first column (which happens to be the bug ids)
        for bug_link_cell in soup('td', "first-child"): # tds with the class "first-child"
            bug_link = bug_link_cell.find("a")
            bug_ids.append(bug_link.string) # the contents happen to be the bug id
    
        return bug_ids

    def fetch_patches_from_commit_queue(self):
        patches_to_land = []
        for bug_id in self.fetch_bug_ids_from_commit_queue():
            patches = self.fetch_reviewed_patches_from_bug(bug_id)
            patches_to_land += patches
        return patches_to_land

    def authenticate(self, username=None, password=None):
        if self.authenticated:
            return
        
        if not username:
            username = read_config("username")
            if not username:
                username = raw_input("Bugzilla login: ")
        if not password:
            password = read_config("password")
            if not password:
                password = getpass.getpass("Bugzilla password for %s: " % username)

        log("Logging in as %s..." % username)
        if self.dryrun:
            self.authenticated = True
            return
        self.browser.open(self.bug_server + "/index.cgi?GoAheadAndLogIn=1")
        self.browser.select_form(name="login")
        self.browser['Bugzilla_login'] = username
        self.browser['Bugzilla_password'] = password
        self.browser.submit()

        # We really should check the result codes and try again as necessary
        self.authenticated = True

    def add_patch_to_bug(self, bug_id, patch_file_object, description, comment_text=None, mark_for_review=False):
        self.authenticate()
        
        log('Adding patch "%s" to bug %s' % (description, bug_id))
        if self.dryrun:
            log(comment_text)
            return
        
        self.browser.open(self.bug_server + "/attachment.cgi?action=enter&bugid=" + bug_id)
        self.browser.select_form(name="entryform")
        self.browser['description'] = description
        self.browser['ispatch'] = ("1",)
        if comment_text:
            log(comment_text)
            self.browser['comment'] = comment_text
        self.browser['flag_type-1'] = ('?',) if mark_for_review else ('X',)
        self.browser.add_file(patch_file_object, "text/plain", "bugzilla_requires_a_filename.patch")
        self.browser.submit()

    def obsolete_attachment(self, attachment_id, comment_text = None):
        self.authenticate()

        log("Obsoleting attachment: %s" % attachment_id)
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.attachment_url_for_id(attachment_id, 'edit'))
        self.browser.select_form(nr=0)
        self.browser.find_control('isobsolete').items[0].selected = True
        # Also clear any review flag (to remove it from review/commit queues)
        self.browser.find_control(type='select', nr=0).value = ("X",)
        if comment_text:
            log(comment_text)
            self.browser['comment'] = comment_text
        self.browser.submit()
    
    def post_comment_to_bug(self, bug_id, comment_text):
        self.authenticate()

        log("Adding comment to bug %s" % bug_id)
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        self.browser['comment'] = comment_text
        self.browser.submit()

    def close_bug_as_fixed(self, bug_id, comment_text=None):
        self.authenticate()

        log("Closing bug %s as fixed" % bug_id)
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        if comment_text:
            log(comment_text)
            self.browser['comment'] = comment_text
        self.browser['knob'] = ['resolve']
        self.browser['resolution'] = ['FIXED']
        self.browser.submit()

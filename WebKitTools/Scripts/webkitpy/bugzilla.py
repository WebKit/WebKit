# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# Copyright (c) 2010 Research In Motion Limited. All rights reserved.
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

import re
import subprocess

from datetime import datetime # used in timestamp()

# Import WebKit-specific modules.
from webkitpy.webkit_logging import error, log
from webkitpy.committers import CommitterList
from webkitpy.credentials import Credentials
from webkitpy.user import User

# WebKit includes a built copy of BeautifulSoup in Scripts/webkitpy
# so this import should always succeed.
from .BeautifulSoup import BeautifulSoup, SoupStrainer

from mechanize import Browser


def parse_bug_id(message):
    match = re.search("http\://webkit\.org/b/(?P<bug_id>\d+)", message)
    if match:
        return int(match.group('bug_id'))
    match = re.search(
        Bugzilla.bug_server_regex + "show_bug\.cgi\?id=(?P<bug_id>\d+)",
        message)
    if match:
        return int(match.group('bug_id'))
    return None


def timestamp():
    return datetime.now().strftime("%Y%m%d%H%M%S")


class Attachment(object):

    def __init__(self, attachment_dictionary, bug):
        self._attachment_dictionary = attachment_dictionary
        self._bug = bug
        self._reviewer = None
        self._committer = None

    def _bugzilla(self):
        return self._bug._bugzilla

    def id(self):
        return int(self._attachment_dictionary.get("id"))

    def attacher_is_committer(self):
        return self._bugzilla.committers.committer_by_email(
            patch.attacher_email())

    def attacher_email(self):
        return self._attachment_dictionary.get("attacher_email")

    def bug(self):
        return self._bug

    def bug_id(self):
        return int(self._attachment_dictionary.get("bug_id"))

    def is_patch(self):
        return not not self._attachment_dictionary.get("is_patch")

    def is_obsolete(self):
        return not not self._attachment_dictionary.get("is_obsolete")

    def name(self):
        return self._attachment_dictionary.get("name")

    def review(self):
        return self._attachment_dictionary.get("review")

    def commit_queue(self):
        return self._attachment_dictionary.get("commit-queue")

    def url(self):
        # FIXME: This should just return
        # self._bugzilla().attachment_url_for_id(self.id()). scm_unittest.py
        # depends on the current behavior.
        return self._attachment_dictionary.get("url")

    def _validate_flag_value(self, flag):
        email = self._attachment_dictionary.get("%s_email" % flag)
        if not email:
            return None
        committer = getattr(self._bugzilla().committers,
                            "%s_by_email" % flag)(email)
        if committer:
            return committer
        log("Warning, attachment %s on bug %s has invalid %s (%s)" % (
                 self._attachment_dictionary['id'],
                 self._attachment_dictionary['bug_id'], flag, email))

    def reviewer(self):
        if not self._reviewer:
            self._reviewer = self._validate_flag_value("reviewer")
        return self._reviewer

    def committer(self):
        if not self._committer:
            self._committer = self._validate_flag_value("committer")
        return self._committer


class Bug(object):
    # FIXME: This class is kinda a hack for now.  It exists so we have one
    # place to hold bug logic, even if much of the code deals with
    # dictionaries still.

    def __init__(self, bug_dictionary, bugzilla):
        self.bug_dictionary = bug_dictionary
        self._bugzilla = bugzilla

    def id(self):
        return self.bug_dictionary["id"]

    def assigned_to_email(self):
        return self.bug_dictionary["assigned_to_email"]

    # Rarely do we actually want obsolete attachments
    def attachments(self, include_obsolete=False):
        attachments = self.bug_dictionary["attachments"]
        if not include_obsolete:
            attachments = filter(lambda attachment:
                                 not attachment["is_obsolete"], attachments)
        return [Attachment(attachment, self) for attachment in attachments]

    def patches(self, include_obsolete=False):
        return [patch for patch in self.attachments(include_obsolete)
                                   if patch.is_patch()]

    def unreviewed_patches(self):
        return [patch for patch in self.patches() if patch.review() == "?"]

    def reviewed_patches(self, include_invalid=False):
        patches = [patch for patch in self.patches() if patch.review() == "+"]
        if include_invalid:
            return patches
        # Checking reviewer() ensures that it was both reviewed and has a valid
        # reviewer.
        return filter(lambda patch: patch.reviewer(), patches)

    def commit_queued_patches(self, include_invalid=False):
        patches = [patch for patch in self.patches()
                                      if patch.commit_queue() == "+"]
        if include_invalid:
            return patches
        # Checking committer() ensures that it was both commit-queue+'d and has
        # a valid committer.
        return filter(lambda patch: patch.committer(), patches)


# A container for all of the logic for making and parsing buzilla queries.
class BugzillaQueries(object):

    def __init__(self, bugzilla):
        self._bugzilla = bugzilla

    # Note: _load_query and _fetch_bug are the only two methods which access
    # self._bugzilla.

    def _load_query(self, query):
        self._bugzilla.authenticate()

        full_url = "%s%s" % (self._bugzilla.bug_server_url, query)
        return self._bugzilla.browser.open(full_url)

    def _fetch_bug(self, bug_id):
        return self._bugzilla.fetch_bug(bug_id)

    def _fetch_bug_ids_advanced_query(self, query):
        soup = BeautifulSoup(self._load_query(query))
        # The contents of the <a> inside the cells in the first column happen
        # to be the bug id.
        return [int(bug_link_cell.find("a").string)
                for bug_link_cell in soup('td', "first-child")]

    def _parse_attachment_ids_request_query(self, page):
        digits = re.compile("\d+")
        attachment_href = re.compile("attachment.cgi\?id=\d+&action=review")
        attachment_links = SoupStrainer("a", href=attachment_href)
        return [int(digits.search(tag["href"]).group(0))
                for tag in BeautifulSoup(page, parseOnlyThese=attachment_links)]

    def _fetch_attachment_ids_request_query(self, query):
        return self._parse_attachment_ids_request_query(self._load_query(query))

    # List of all r+'d bugs.
    def fetch_bug_ids_from_pending_commit_list(self):
        needs_commit_query_url = "buglist.cgi?query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&field0-0-0=flagtypes.name&type0-0-0=equals&value0-0-0=review%2B"
        return self._fetch_bug_ids_advanced_query(needs_commit_query_url)

    def fetch_patches_from_pending_commit_list(self):
        return sum([self._fetch_bug(bug_id).reviewed_patches()
            for bug_id in self.fetch_bug_ids_from_pending_commit_list()], [])

    def fetch_bug_ids_from_commit_queue(self):
        commit_queue_url = "buglist.cgi?query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&field0-0-0=flagtypes.name&type0-0-0=equals&value0-0-0=commit-queue%2B&order=Last+Changed"
        return self._fetch_bug_ids_advanced_query(commit_queue_url)

    def fetch_patches_from_commit_queue(self):
        # This function will only return patches which have valid committers
        # set.  It won't reject patches with invalid committers/reviewers.
        return sum([self._fetch_bug(bug_id).commit_queued_patches()
                    for bug_id in self.fetch_bug_ids_from_commit_queue()], [])

    def _fetch_bug_ids_from_review_queue(self):
        review_queue_url = "buglist.cgi?query_format=advanced&bug_status=UNCONFIRMED&bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&field0-0-0=flagtypes.name&type0-0-0=equals&value0-0-0=review?"
        return self._fetch_bug_ids_advanced_query(review_queue_url)

    def fetch_patches_from_review_queue(self, limit=None):
        # [:None] returns the whole array.
        return sum([self._fetch_bug(bug_id).unreviewed_patches()
            for bug_id in self._fetch_bug_ids_from_review_queue()[:limit]], [])

    # FIXME: Why do we have both fetch_patches_from_review_queue and
    # fetch_attachment_ids_from_review_queue??
    # NOTE: This is also the only client of _fetch_attachment_ids_request_query

    def fetch_attachment_ids_from_review_queue(self):
        review_queue_url = "request.cgi?action=queue&type=review&group=type"
        return self._fetch_attachment_ids_request_query(review_queue_url)


class CommitterValidator(object):

    def __init__(self, bugzilla):
        self._bugzilla = bugzilla

    # _view_source_url belongs in some sort of webkit_config.py module.
    def _view_source_url(self, local_path):
        return "http://trac.webkit.org/browser/trunk/%s" % local_path

    def _flag_permission_rejection_message(self, setter_email, flag_name):
        # This could be computed from CommitterList.__file__
        committer_list = "WebKitTools/Scripts/webkitpy/committers.py"
        # Should come from some webkit_config.py
        contribution_guidlines = "http://webkit.org/coding/contributing.html"
        # This could be queried from the status_server.
        queue_administrator = "eseidel@chromium.org"
        # This could be queried from the tool.
        queue_name = "commit-queue"
        message = "%s does not have %s permissions according to %s." % (
                        setter_email,
                        flag_name,
                        self._view_source_url(committer_list))
        message += "\n\n- If you do not have %s rights please read %s for instructions on how to use bugzilla flags." % (
                        flag_name, contribution_guidlines)
        message += "\n\n- If you have %s rights please correct the error in %s by adding yourself to the file (no review needed).  " % (
                        flag_name, committer_list)
        message += "Due to bug 30084 the %s will require a restart after your change.  " % queue_name
        message += "Please contact %s to request a %s restart.  " % (
                        queue_administrator, queue_name)
        message += "After restart the %s will correctly respect your %s rights." % (
                        queue_name, flag_name)
        return message

    def _validate_setter_email(self, patch, result_key, rejection_function):
        committer = getattr(patch, result_key)()
        # If the flag is set, and we don't recognize the setter, reject the
        # flag!
        setter_email = patch._attachment_dictionary.get("%s_email" % result_key)
        if setter_email and not committer:
            rejection_function(patch.id(),
                self._flag_permission_rejection_message(setter_email,
                                                        result_key))
            return False
        return True

    def patches_after_rejecting_invalid_commiters_and_reviewers(self, patches):
        validated_patches = []
        for patch in patches:
            if (self._validate_setter_email(
                    patch, "reviewer", self.reject_patch_from_review_queue)
                and self._validate_setter_email(
                    patch, "committer", self.reject_patch_from_commit_queue)):
                validated_patches.append(patch)
        return validated_patches

    def reject_patch_from_commit_queue(self,
                                       attachment_id,
                                       additional_comment_text=None):
        comment_text = "Rejecting patch %s from commit-queue." % attachment_id
        self._bugzilla.set_flag_on_attachment(attachment_id,
                                              "commit-queue",
                                              "-",
                                              comment_text,
                                              additional_comment_text)

    def reject_patch_from_review_queue(self,
                                       attachment_id,
                                       additional_comment_text=None):
        comment_text = "Rejecting patch %s from review queue." % attachment_id
        self._bugzilla.set_flag_on_attachment(attachment_id,
                                              'review',
                                              '-',
                                              comment_text,
                                              additional_comment_text)


class Bugzilla(object):

    def __init__(self, dryrun=False, committers=CommitterList()):
        self.dryrun = dryrun
        self.authenticated = False
        self.queries = BugzillaQueries(self)
        self.committers = committers

        # FIXME: We should use some sort of Browser mock object when in dryrun
        # mode (to prevent any mistakes).
        self.browser = Browser()
        # Ignore bugs.webkit.org/robots.txt until we fix it to allow this
        # script.
        self.browser.set_handle_robots(False)

    # FIXME: Much of this should go into some sort of config module:
    bug_server_host = "bugs.webkit.org"
    bug_server_regex = "https?://%s/" % re.sub('\.', '\\.', bug_server_host)
    bug_server_url = "https://%s/" % bug_server_host
    unassigned_email = "webkit-unassigned@lists.webkit.org"

    def bug_url_for_bug_id(self, bug_id, xml=False):
        content_type = "&ctype=xml" if xml else ""
        return "%sshow_bug.cgi?id=%s%s" % (self.bug_server_url,
                                           bug_id,
                                           content_type)

    def short_bug_url_for_bug_id(self, bug_id):
        return "http://webkit.org/b/%s" % bug_id

    def attachment_url_for_id(self, attachment_id, action="view"):
        action_param = ""
        if action and action != "view":
            action_param = "&action=%s" % action
        return "%sattachment.cgi?id=%s%s" % (self.bug_server_url,
                                             attachment_id,
                                             action_param)

    def _parse_attachment_flag(self,
                               element,
                               flag_name,
                               attachment,
                               result_key):
        flag = element.find('flag', attrs={'name': flag_name})
        if flag:
            attachment[flag_name] = flag['status']
            if flag['status'] == '+':
                attachment[result_key] = flag['setter']

    def _parse_attachment_element(self, element, bug_id):
        attachment = {}
        attachment['bug_id'] = bug_id
        attachment['is_obsolete'] = (element.has_key('isobsolete') and element['isobsolete'] == "1")
        attachment['is_patch'] = (element.has_key('ispatch') and element['ispatch'] == "1")
        attachment['id'] = int(element.find('attachid').string)
        # FIXME: No need to parse out the url here.
        attachment['url'] = self.attachment_url_for_id(attachment['id'])
        attachment['name'] = unicode(element.find('desc').string)
        attachment['attacher_email'] = str(element.find('attacher').string)
        attachment['type'] = str(element.find('type').string)
        self._parse_attachment_flag(
                element, 'review', attachment, 'reviewer_email')
        self._parse_attachment_flag(
                element, 'commit-queue', attachment, 'committer_email')
        return attachment

    def _parse_bug_page(self, page):
        soup = BeautifulSoup(page)
        bug = {}
        bug["id"] = int(soup.find("bug_id").string)
        bug["title"] = unicode(soup.find("short_desc").string)
        bug["reporter_email"] = str(soup.find("reporter").string)
        bug["assigned_to_email"] = str(soup.find("assigned_to").string)
        bug["cc_emails"] = [str(element.string)
                            for element in soup.findAll('cc')]
        bug["attachments"] = [self._parse_attachment_element(element, bug["id"]) for element in soup.findAll('attachment')]
        return bug

    # Makes testing fetch_*_from_bug() possible until we have a better
    # BugzillaNetwork abstration.

    def _fetch_bug_page(self, bug_id):
        bug_url = self.bug_url_for_bug_id(bug_id, xml=True)
        log("Fetching: %s" % bug_url)
        return self.browser.open(bug_url)

    def fetch_bug_dictionary(self, bug_id):
        return self._parse_bug_page(self._fetch_bug_page(bug_id))

    # FIXME: A BugzillaCache object should provide all these fetch_ methods.

    def fetch_bug(self, bug_id):
        return Bug(self.fetch_bug_dictionary(bug_id), self)

    def _parse_bug_id_from_attachment_page(self, page):
        # The "Up" relation happens to point to the bug.
        up_link = BeautifulSoup(page).find('link', rel='Up')
        if not up_link:
            # This attachment does not exist (or you don't have permissions to
            # view it).
            return None
        match = re.search("show_bug.cgi\?id=(?P<bug_id>\d+)", up_link['href'])
        return int(match.group('bug_id'))

    def bug_id_for_attachment_id(self, attachment_id):
        self.authenticate()

        attachment_url = self.attachment_url_for_id(attachment_id, 'edit')
        log("Fetching: %s" % attachment_url)
        page = self.browser.open(attachment_url)
        return self._parse_bug_id_from_attachment_page(page)

    # FIXME: This should just return Attachment(id), which should be able to
    # lazily fetch needed data.

    def fetch_attachment(self, attachment_id):
        # We could grab all the attachment details off of the attachment edit
        # page but we already have working code to do so off of the bugs page,
        # so re-use that.
        bug_id = self.bug_id_for_attachment_id(attachment_id)
        if not bug_id:
            return None
        attachments = self.fetch_bug(bug_id).attachments(include_obsolete=True)
        for attachment in attachments:
            if attachment.id() == int(attachment_id):
                return attachment
        return None # This should never be hit.

    def authenticate(self):
        if self.authenticated:
            return

        if self.dryrun:
            log("Skipping log in for dry run...")
            self.authenticated = True
            return

        attempts = 0
        while not self.authenticated:
            attempts += 1
            (username, password) = Credentials(
                self.bug_server_host, git_prefix="bugzilla").read_credentials()

            log("Logging in as %s..." % username)
            self.browser.open(self.bug_server_url +
                              "index.cgi?GoAheadAndLogIn=1")
            self.browser.select_form(name="login")
            self.browser['Bugzilla_login'] = username
            self.browser['Bugzilla_password'] = password
            response = self.browser.submit()

            match = re.search("<title>(.+?)</title>", response.read())
            # If the resulting page has a title, and it contains the word
            # "invalid" assume it's the login failure page.
            if match and re.search("Invalid", match.group(1), re.IGNORECASE):
                errorMessage = "Bugzilla login failed: %s" % match.group(1)
                # raise an exception only if this was the last attempt
                if attempts < 5:
                    log(errorMessage)
                else:
                    raise Exception(errorMessage)
            else:
                self.authenticated = True

    def _fill_attachment_form(self,
                              description,
                              patch_file_object,
                              comment_text=None,
                              mark_for_review=False,
                              mark_for_commit_queue=False,
                              mark_for_landing=False, bug_id=None):
        self.browser['description'] = description
        self.browser['ispatch'] = ("1",)
        self.browser['flag_type-1'] = ('?',) if mark_for_review else ('X',)

        if mark_for_landing:
            self.browser['flag_type-3'] = ('+',)
        elif mark_for_commit_queue:
            self.browser['flag_type-3'] = ('?',)
        else:
            self.browser['flag_type-3'] = ('X',)

        if bug_id:
            patch_name = "bug-%s-%s.patch" % (bug_id, timestamp())
        else:
            patch_name ="%s.patch" % timestamp()
        self.browser.add_file(patch_file_object,
                              "text/plain",
                              patch_name,
                              'data')

    def add_patch_to_bug(self,
                         bug_id,
                         patch_file_object,
                         description,
                         comment_text=None,
                         mark_for_review=False,
                         mark_for_commit_queue=False,
                         mark_for_landing=False):
        self.authenticate()

        log('Adding patch "%s" to %sshow_bug.cgi?id=%s' % (description,
                                                           self.bug_server_url,
                                                           bug_id))

        if self.dryrun:
            log(comment_text)
            return

        self.browser.open("%sattachment.cgi?action=enter&bugid=%s" % (
                          self.bug_server_url, bug_id))
        self.browser.select_form(name="entryform")
        self._fill_attachment_form(description,
                                   patch_file_object,
                                   mark_for_review=mark_for_review,
                                   mark_for_commit_queue=mark_for_commit_queue,
                                   mark_for_landing=mark_for_landing,
                                   bug_id=bug_id)
        if comment_text:
            log(comment_text)
            self.browser['comment'] = comment_text
        self.browser.submit()

    def prompt_for_component(self, components):
        log("Please pick a component:")
        i = 0
        for name in components:
            i += 1
            log("%2d. %s" % (i, name))
        result = int(User.prompt("Enter a number: ")) - 1
        return components[result]

    def _check_create_bug_response(self, response_html):
        match = re.search("<title>Bug (?P<bug_id>\d+) Submitted</title>",
                          response_html)
        if match:
            return match.group('bug_id')

        match = re.search(
            '<div id="bugzilla-body">(?P<error_message>.+)<div id="footer">',
            response_html,
            re.DOTALL)
        error_message = "FAIL"
        if match:
            text_lines = BeautifulSoup(
                    match.group('error_message')).findAll(text=True)
            error_message = "\n" + '\n'.join(
                    ["  " + line.strip()
                     for line in text_lines if line.strip()])
        raise Exception("Bug not created: %s" % error_message)

    def create_bug(self,
                   bug_title,
                   bug_description,
                   component=None,
                   patch_file_object=None,
                   patch_description=None,
                   cc=None,
                   mark_for_review=False,
                   mark_for_commit_queue=False):
        self.authenticate()

        log('Creating bug with title "%s"' % bug_title)
        if self.dryrun:
            log(bug_description)
            return

        self.browser.open(self.bug_server_url + "enter_bug.cgi?product=WebKit")
        self.browser.select_form(name="Create")
        component_items = self.browser.find_control('component').items
        component_names = map(lambda item: item.name, component_items)
        if not component:
            component = "New Bugs"
        if component not in component_names:
            component = self.prompt_for_component(component_names)
        self.browser['component'] = [component]
        if cc:
            self.browser['cc'] = cc
        self.browser['short_desc'] = bug_title
        self.browser['comment'] = bug_description

        if patch_file_object:
            self._fill_attachment_form(
                    patch_description,
                    patch_file_object,
                    mark_for_review=mark_for_review,
                    mark_for_commit_queue=mark_for_commit_queue)

        response = self.browser.submit()

        bug_id = self._check_create_bug_response(response.read())
        log("Bug %s created." % bug_id)
        log("%sshow_bug.cgi?id=%s" % (self.bug_server_url, bug_id))
        return bug_id

    def _find_select_element_for_flag(self, flag_name):
        # FIXME: This will break if we ever re-order attachment flags
        if flag_name == "review":
            return self.browser.find_control(type='select', nr=0)
        if flag_name == "commit-queue":
            return self.browser.find_control(type='select', nr=1)
        raise Exception("Don't know how to find flag named \"%s\"" % flag_name)

    def clear_attachment_flags(self,
                               attachment_id,
                               additional_comment_text=None):
        self.authenticate()

        comment_text = "Clearing flags on attachment: %s" % attachment_id
        if additional_comment_text:
            comment_text += "\n\n%s" % additional_comment_text
        log(comment_text)

        if self.dryrun:
            return

        self.browser.open(self.attachment_url_for_id(attachment_id, 'edit'))
        self.browser.select_form(nr=1)
        self.browser.set_value(comment_text, name='comment', nr=0)
        self._find_select_element_for_flag('review').value = ("X",)
        self._find_select_element_for_flag('commit-queue').value = ("X",)
        self.browser.submit()

    def set_flag_on_attachment(self,
                               attachment_id,
                               flag_name,
                               flag_value,
                               comment_text,
                               additional_comment_text):
        # FIXME: We need a way to test this function on a live bugzilla
        # instance.

        self.authenticate()

        if additional_comment_text:
            comment_text += "\n\n%s" % additional_comment_text
        log(comment_text)

        if self.dryrun:
            return

        self.browser.open(self.attachment_url_for_id(attachment_id, 'edit'))
        self.browser.select_form(nr=1)
        self.browser.set_value(comment_text, name='comment', nr=0)
        self._find_select_element_for_flag(flag_name).value = (flag_value,)
        self.browser.submit()

    # FIXME: All of these bug editing methods have a ridiculous amount of
    # copy/paste code.

    def obsolete_attachment(self, attachment_id, comment_text=None):
        self.authenticate()

        log("Obsoleting attachment: %s" % attachment_id)
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.attachment_url_for_id(attachment_id, 'edit'))
        self.browser.select_form(nr=1)
        self.browser.find_control('isobsolete').items[0].selected = True
        # Also clear any review flag (to remove it from review/commit queues)
        self._find_select_element_for_flag('review').value = ("X",)
        self._find_select_element_for_flag('commit-queue').value = ("X",)
        if comment_text:
            log(comment_text)
            # Bugzilla has two textareas named 'comment', one is somehow
            # hidden.  We want the first.
            self.browser.set_value(comment_text, name='comment', nr=0)
        self.browser.submit()

    def add_cc_to_bug(self, bug_id, email_address_list):
        self.authenticate()

        log("Adding %s to the CC list for bug %s" % (email_address_list,
                                                     bug_id))
        if self.dryrun:
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        self.browser["newcc"] = ", ".join(email_address_list)
        self.browser.submit()

    def post_comment_to_bug(self, bug_id, comment_text, cc=None):
        self.authenticate()

        log("Adding comment to bug %s" % bug_id)
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        self.browser["comment"] = comment_text
        if cc:
            self.browser["newcc"] = ", ".join(cc)
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
        self.browser['bug_status'] = ['RESOLVED']
        self.browser['resolution'] = ['FIXED']
        self.browser.submit()

    def reassign_bug(self, bug_id, assignee, comment_text=None):
        self.authenticate()

        log("Assigning bug %s to %s" % (bug_id, assignee))
        if self.dryrun:
            log(comment_text)
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        if comment_text:
            log(comment_text)
            self.browser["comment"] = comment_text
        self.browser["assigned_to"] = assignee
        self.browser.submit()

    def reopen_bug(self, bug_id, comment_text):
        self.authenticate()

        log("Re-opening bug %s" % bug_id)
        # Bugzilla requires a comment when re-opening a bug, so we know it will
        # never be None.
        log(comment_text)
        if self.dryrun:
            return

        self.browser.open(self.bug_url_for_bug_id(bug_id))
        self.browser.select_form(name="changeform")
        bug_status = self.browser.find_control("bug_status", type="select")
        # This is a hack around the fact that ClientForm.ListControl seems to
        # have no simpler way to ask if a control has an item named "REOPENED"
        # without using exceptions for control flow.
        possible_bug_statuses = map(lambda item: item.name, bug_status.items)
        if "REOPENED" in possible_bug_statuses:
            bug_status.value = ["REOPENED"]
        else:
            log("Did not reopen bug %s.  " +
                "It appears to already be open with status %s." % (
                        bug_id, bug_status.value))
        self.browser['comment'] = comment_text
        self.browser.submit()

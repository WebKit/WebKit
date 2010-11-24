# Copyright (C) 2010 Google Inc. All rights reserved.
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

import logging
import urllib

from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template

from model.jsonresults import JsonResults
from model.testfile import TestFile

PARAM_MASTER = "master"
PARAM_BUILDER = "builder"
PARAM_DIR = "dir"
PARAM_FILE = "file"
PARAM_NAME = "name"
PARAM_KEY = "key"
PARAM_TEST_TYPE = "testtype"
PARAM_INCREMENTAL = "incremental"
PARAM_TEST_LIST_JSON = "testlistjson"


class DeleteFile(webapp.RequestHandler):
    """Delete test file for a given builder and name from datastore."""

    def get(self):
        key = self.request.get(PARAM_KEY)
        master = self.request.get(PARAM_MASTER)
        builder = self.request.get(PARAM_BUILDER)
        test_type = self.request.get(PARAM_TEST_TYPE)
        name = self.request.get(PARAM_NAME)

        logging.debug(
            "Deleting File, master: %s, builder: %s, test_type: %s, name: %s, key: %s.",
            master, builder, test_type, name, key)

        TestFile.delete_file(key, master, builder, test_type, name, 100)

        # Display file list after deleting the file.
        self.redirect("/testfile?master=%s&builder=%s&testtype=%s&name=%s"
            % (master, builder, test_type, name))


class GetFile(webapp.RequestHandler):
    """Get file content or list of files for given builder and name."""

    def _get_file_list(self, master, builder, test_type, name):
        """Get and display a list of files that matches builder and file name.

        Args:
            builder: builder name
            test_type: type of the test
            name: file name
        """

        files = TestFile.get_files(
            master, builder, test_type, name, load_data=False, limit=100)
        if not files:
            logging.info("File not found, master: %s, builder: %s, test_type: %s, name: %s.",
                         master, builder, test_type, name)
            self.response.out.write("File not found")
            return

        template_values = {
            "admin": users.is_current_user_admin(),
            "master": master,
            "builder": builder,
            "test_type": test_type,
            "name": name,
            "files": files,
        }
        self.response.out.write(template.render("templates/showfilelist.html",
                                                template_values))

    def _get_file_content(self, master, builder, test_type, name):
        """Return content of the file that matches builder and file name.

        Args:
            builder: builder name
            test_type: type of the test
            name: file name
        """

        files = TestFile.get_files(
            master, builder, test_type, name, load_data=True, limit=1)
        if not files:
            logging.info("File not found, master %s, builder: %s, test_type: %s, name: %s.",
                         master, builder, test_type, name)
            return None

        return files[0].data

    def _get_test_list_json(self, master, builder, test_type):
        """Return json file with test name list only, do not include test
           results and other non-test-data .

        Args:
            builder: builder name.
            test_type: type of test results.
        """

        json = self._get_file_content(master, builder, test_type, "results.json")
        if not json:
            return None

        return JsonResults.get_test_list(builder, json)

    def get(self):
        master = self.request.get(PARAM_MASTER)
        builder = self.request.get(PARAM_BUILDER)
        test_type = self.request.get(PARAM_TEST_TYPE)
        name = self.request.get(PARAM_NAME)
        dir = self.request.get(PARAM_DIR)
        test_list_json = self.request.get(PARAM_TEST_LIST_JSON)

        logging.debug(
            "Getting files, master %s, builder: %s, test_type: %s, name: %s.",
            master, builder, test_type, name)

        # If parameter "dir" is specified or there is no builder or filename
        # specified in the request, return list of files, otherwise, return
        # file content.
        if dir or not builder or not name:
            return self._get_file_list(master, builder, test_type, name)

        if name == "results.json" and test_list_json:
            json = self._get_test_list_json(master, builder, test_type)
        else:
            json = self._get_file_content(master, builder, test_type, name)

        if json:
            self.response.headers["Content-Type"] = "text/plain; charset=utf-8"
            self.response.out.write(json)


class Upload(webapp.RequestHandler):
    """Upload test results file to datastore."""

    def post(self):
        file_params = self.request.POST.getall(PARAM_FILE)
        if not file_params:
            self.response.out.write("FAIL: missing upload file field.")
            return

        builder = self.request.get(PARAM_BUILDER)
        if not builder:
            self.response.out.write("FAIL: missing builder parameter.")
            return

        master = self.request.get(PARAM_MASTER)
        test_type = self.request.get(PARAM_TEST_TYPE)
        incremental = self.request.get(PARAM_INCREMENTAL)

        logging.debug(
            "Processing upload request, master: %s, builder: %s, test_type: %s.",
            master, builder, test_type)

        # There are two possible types of each file_params in the request:
        # one file item or a list of file items.
        # Normalize file_params to a file item list.
        files = []
        logging.debug("test: %s, type:%s", file_params, type(file_params))
        for item in file_params:
            if not isinstance(item, list) and not isinstance(item, tuple):
                item = [item]
            files.extend(item)

        errors = []
        for file in files:
            filename = file.filename.lower()
            if ((incremental and filename == "results.json") or
                (filename == "incremental_results.json")):
                # Merge incremental json results.
                update_succeeded = JsonResults.update(master, builder, test_type, file.value)
            else:
                update_succeeded = TestFile.update(
                    master, builder, test_type, file.filename, file.value)

            if not update_succeeded:
                errors.append(
                    "Upload failed, master: %s, builder: %s, test_type: %s, name: %s." %
                    (master, builder, test_type, file.filename))

        if errors:
            messages = "FAIL: " + "; ".join(errors)
            logging.warning(messages)
            self.response.set_status(500, messages)
            self.response.out.write("FAIL")
        else:
            self.response.set_status(200)
            self.response.out.write("OK")


class UploadForm(webapp.RequestHandler):
    """Show a form so user can upload a file."""

    def get(self):
        template_values = {
            "upload_url": "/testfile/upload",
        }
        self.response.out.write(template.render("templates/uploadform.html",
                                                template_values))

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
from google.appengine.ext import blobstore
from google.appengine.ext import webapp
from google.appengine.ext.webapp import blobstore_handlers
from google.appengine.ext.webapp import template

from model.testfile import TestFile

PARAM_BUILDER = "builder"
PARAM_DIR = "dir"
PARAM_FILE = "file"
PARAM_NAME = "name"
PARAM_KEY = "key"
PARAM_TEST_TYPE = "testtype"


class DeleteFile(webapp.RequestHandler):
    """Delete test file for a given builder and name from datastore (metadata) and blobstore (file data)."""

    def get(self):
        key = self.request.get(PARAM_KEY)
        builder = self.request.get(PARAM_BUILDER)
        test_type = self.request.get(PARAM_TEST_TYPE)
        name = self.request.get(PARAM_NAME)

        logging.debug(
            "Deleting File, builder: %s, test_type: %s, name: %s, blob key: %s.",
            builder, test_type, name, key)

        TestFile.delete_file(key, builder, test_type, name, 100)

        # Display file list after deleting the file.
        self.redirect("/testfile?builder=%s&testtype=%s&name=%s"
            % (builder, test_type, name))


class GetFile(blobstore_handlers.BlobstoreDownloadHandler):
    """Get file content or list of files for given builder and name."""

    def _get_file_list(self, builder, test_type, name):
        """Get and display a list of files that matches builder and file name.

        Args:
            builder: builder name
            test_type: type of the test
            name: file name
        """

        files = TestFile.get_files(builder, test_type, name, 100)
        if not files:
            logging.info("File not found, builder: %s, test_type: %s, name: %s.",
                         builder, test_type, name)
            self.response.out.write("File not found")
            return

        template_values = {
            "admin": users.is_current_user_admin(),
            "builder": builder,
            "test_type": test_type,
            "name": name,
            "files": files,
        }
        self.response.out.write(template.render("templates/showfilelist.html",
                                                template_values))

    def _get_file_content(self, builder, test_type, name):
        """Return content of the file that matches builder and file name.

        Args:
            builder: builder name
            test_type: type of the test
            name: file name
        """

        files = TestFile.get_files(builder, test_type, name, 1)
        if not files:
            logging.info("File not found, builder: %s, test_type: %s, name: %s.",
                         builder, test_type, name)
            return

        blob_key = files[0].blob_key
        blob_info = blobstore.get(blob_key)
        if blob_info:
            self.send_blob(blob_info, "text/plain")

    def get(self):
        builder = self.request.get(PARAM_BUILDER)
        test_type = self.request.get(PARAM_TEST_TYPE)
        name = self.request.get(PARAM_NAME)
        dir = self.request.get(PARAM_DIR)

        logging.debug(
            "Getting files, builder: %s, test_type: %s, name: %s.",
            builder, test_type, name)

        # If parameter "dir" is specified or there is no builder or filename
        # specified in the request, return list of files, otherwise, return
        # file content.
        if dir or not builder or not name:
            return self._get_file_list(builder, test_type, name)
        else:
            return self._get_file_content(builder, test_type, name)


class GetUploadUrl(webapp.RequestHandler):
    """Get an url for uploading file to blobstore. A special url is required for each blobsotre upload."""

    def get(self):
        upload_url = blobstore.create_upload_url("/testfile/upload")
        logging.info("Getting upload url: %s.", upload_url)
        self.response.out.write(upload_url)


class Upload(blobstore_handlers.BlobstoreUploadHandler):
    """Upload file to blobstore."""

    def post(self):
        uploaded_files = self.get_uploads("file")
        if not uploaded_files:
            return self._upload_done([("Missing upload file field.")])

        builder = self.request.get(PARAM_BUILDER)
        if not builder:
            for blob_info in uploaded_files:
                blob_info.delete()
    
            return self._upload_done([("Missing builder parameter in upload request.")])

        test_type = self.request.get(PARAM_TEST_TYPE)

        logging.debug(
            "Processing upload request, builder: %s, test_type: %s.",
            builder, test_type)

        errors = []
        for blob_info in uploaded_files:
            tf = TestFile.update_file(builder, test_type, blob_info)
            if not tf:
                errors.append(
                    "Upload failed, builder: %s, test_type: %s, name: %s." %
                    (builder, test_type, blob_info.filename))
                blob_info.delete()

        return self._upload_done(errors)

    def _upload_done(self, errors):
        logging.info("upload done.")

        error_messages = []
        for error in errors:
            logging.info(error)
            error_messages.append("error=%s" % urllib.quote(error))

        if error_messages:
            redirect_url = "/uploadfail?%s" % "&".join(error_messages)
        else:
            redirect_url = "/uploadsuccess"

        logging.info(redirect_url)
        # BlobstoreUploadHandler requires redirect at the end.
        self.redirect(redirect_url)


class UploadForm(webapp.RequestHandler):
    """Show a form so user can submit a file to blobstore."""

    def get(self):
        upload_url = blobstore.create_upload_url("/testfile/upload")
        template_values = {
            "upload_url": upload_url,
        }
        self.response.out.write(template.render("templates/uploadform.html",
                                                template_values))

class UploadStatus(webapp.RequestHandler):
    """Return status of file uploading"""

    def get(self):
        logging.debug("Update status")

        if self.request.path == "/uploadsuccess":
            self.response.set_status(200)
            self.response.out.write("OK")
        else:
            errors = self.request.params.getall("error")
            if errors:
                messages = "FAIL: " + "; ".join(errors)
                logging.warning(messages)
                self.response.set_status(500, messages)
                self.response.out.write("FAIL")

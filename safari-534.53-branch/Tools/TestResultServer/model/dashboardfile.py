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

from datetime import datetime
import logging
import urllib
import urllib2

from google.appengine.ext import db

SVN_PATH_DASHBOARD = ("http://src.chromium.org/viewvc/chrome/trunk/tools/"
    "dashboards/")


class DashboardFile(db.Model):
    name = db.StringProperty()
    data = db.BlobProperty()
    date = db.DateTimeProperty(auto_now_add=True)

    @classmethod
    def get_files(cls, name, limit=1):
        query = DashboardFile.all()
        if name:
            query = query.filter("name =", name)
        return query.order("-date").fetch(limit)

    @classmethod
    def add_file(cls, name, data):
        file = DashboardFile()
        file.name = name
        file.data = db.Blob(data)
        file.put()

        logging.debug("Dashboard file saved, name: %s.", name)

        return file

    @classmethod
    def grab_file_from_svn(cls, name):
        logging.debug("Grab file from SVN, name: %s.", name)

        url = SVN_PATH_DASHBOARD + urllib.quote_plus(name)

        logging.info("Grab file from SVN, url: %s.", url)
        try:
            file = urllib2.urlopen(url)
            if not file:
                logging.error("Failed to grab dashboard file: %s.", url)
                return None

            return file.read()
        except urllib2.HTTPError, e:
            logging.error("Failed to grab dashboard file: %s", str(e))
        except urllib2.URLError, e:
            logging.error("Failed to grab dashboard file: %s", str(e))

        return None

    @classmethod
    def update_file(cls, name):
        data = cls.grab_file_from_svn(name)
        if not data:
            return False

        logging.info("Got file from SVN.")

        files = cls.get_files(name)
        if not files:
            logging.info("No existing file, added as new file.")
            if cls.add_file(name, data):
                return True
            return False

        logging.debug("Updating existing file.")
        file = files[0]
        file.data = data
        file.date = datetime.now()
        file.put()

        logging.info("Dashboard file replaced, name: %s.", name)

        return True

    @classmethod
    def delete_file(cls, name):
        files = cls.get_files(name)
        if not files:
            logging.warning("File not found, name: %s.", name)
            return False

        for file in files:
            file.delete()

        return True

# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2014 Apple Inc. All rights reserved.
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

# Request a modern Django
from google.appengine.dist import use_library
use_library('django', '1.2')  # Must agree with __init.py__

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app

from handlers.activebots import ActiveBots
from handlers.fetchattachment import FetchAttachment
from handlers.gc import GC
from handlers.nextpatch import NextPatch
from handlers.patch import Patch
from handlers.patchstatus import PatchStatus
from handlers.processingtimesjson import ProcessingTimesJSON
from handlers.queuecharts import QueueCharts
from handlers.queuelengthjson import QueueLengthJSON
from handlers.queuestatus import QueueStatus
from handlers.queuestatusjson import QueueStatusJSON
from handlers.recentstatus import QueuesOverview
from handlers.releasepatch import ReleasePatch
from handlers.releaselock import ReleaseLock
from handlers.showresults import ShowResults
from handlers.statusbubble import StatusBubble
from handlers.submittoews import SubmitToEWS
from handlers.svnrevision import SVNRevision
from handlers.syncqueuelogs import SyncQueueLogs
from handlers.updatestatus import UpdateStatus
from handlers.updatesvnrevision import UpdateSVNRevision
from handlers.updateworkitems import UpdateWorkItems
from handlers.uploadattachment import UploadAttachment


webapp.template.register_template_library('filters.webkit_extras')

routes = [
    ('/', QueuesOverview),
    ('/gc', GC),
    ('/sync-queue-logs', SyncQueueLogs),
    (r'/patch-status/(.*)/(.*)', PatchStatus),
    (r'/patch/(.*)/(.*)', Patch),
    (r'/patch/(.*)', Patch),
    ('/submit-to-ews', SubmitToEWS),
    (r'/results/(.*)', ShowResults),
    (r'/status-bubble/(.*)', StatusBubble),
    (r'/svn-revision/(.*)', SVNRevision),
    (r'/queue-charts/(.*)', QueueCharts),
    (r'/queue-length-json/(.*)', QueueLengthJSON),
    (r'/queue-status/(.*)/bots/(.*)', QueueStatus),
    (r'/queue-status/(.*)', QueueStatus),
    (r'/queue-status-json/(.*)', QueueStatusJSON),
    (r'/next-patch/(.*)', NextPatch),
    (r'/attachment/(.*)/(.*)', FetchAttachment),
    ('/release-patch', ReleasePatch),
    ('/release-lock', ReleaseLock),
    ('/update-status', UpdateStatus),
    ('/update-work-items', UpdateWorkItems),
    ('/update-svn-revision', UpdateSVNRevision),
    ('/upload-attachment', UploadAttachment),
    ('/active-bots', ActiveBots),
    (r'/processing-times-json/(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)\-(\d+)', ProcessingTimesJSON),
]

application = webapp.WSGIApplication(routes, debug=True)

def main():
    run_wsgi_app(application)

if __name__ == "__main__":
    main()

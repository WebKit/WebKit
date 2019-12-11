# Copyright (C) 2014 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
# conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
# of conditions and the following disclaimer in the documentation and/or other materials
# provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


from google.appengine.ext import webapp, db
from google.appengine.ext.webapp import template

from config.queues import work_item_lock_timeout
from handlers.updatebase import UpdateBase
from model.queues import Queue


class ReleaseLock(UpdateBase):
    def get(self):
        self.response.out.write(template.render("templates/releaselock.html", {"timeout": work_item_lock_timeout}))

    def post(self):
        queue_name = self.request.get("queue_name")
        # FIXME: This queue lookup should be shared between handlers.
        queue = Queue.queue_with_name(queue_name)
        if not queue:
            self.error(404)
            return

        attachment_id = self._int_from_request("attachment_id")
        queue.active_work_items().expire_item(attachment_id)

        # ReleaseLock is used when a queue neither succeeded nor failed, so it silently releases the patch.
        # Let's try other patches before retrying this one, in the interest of fairness, and also because
        # another patch could be posted to address queue problems.
        queue.work_items().move_to_end(attachment_id)

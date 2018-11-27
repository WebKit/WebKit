# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import unicode_literals

import logging

from django.db import models

from ews.config import *

_log = logging.getLogger(__name__)


class Patch(models.Model):
    patch_id = models.IntegerField(primary_key=True)
    bug_id = models.IntegerField()
    obsolete = models.BooleanField(default=False)
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return str(self.patch_id)

    @classmethod
    def save_patch(cls, patch_id, bug_id=-1, obsolete=False):
        if not Patch.is_valid_patch_id(patch_id):
            return ERR_INVALID_PATCH_ID

        if Patch.is_existing_patch_id(patch_id):
            _log.info("Patch id {} already exists in database. Skipped saving.".format(patch_id))
            return ERR_EXISTING_PATCH
        Patch(patch_id, bug_id, obsolete).save()
        _log.info('Saved patch in database, id: {}'.format(patch_id))
        return SUCCESS

    @classmethod
    def save_patches(cls, patch_id_list):
        for patch_id in patch_id_list:
            Patch.save_patch(patch_id)

    @classmethod
    def is_valid_patch_id(cls, patch_id):
        if not patch_id or type(patch_id) != int or patch_id < 1:
            _log.warn('Invalid patch id: {}'.format(patch_id))
            return False
        return True

    @classmethod
    def is_existing_patch_id(cls, patch_id):
        return bool(Patch.objects.filter(patch_id=patch_id))

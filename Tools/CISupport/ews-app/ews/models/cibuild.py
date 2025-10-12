# Copyright (C) 2025 Apple Inc. All rights reserved.
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
from ews.config import ERR_UNEXPECTED, SUCCESS
from ews.models.patch import Change

_log = logging.getLogger(__name__)


class CIBuild(models.Model):
    change = models.ForeignKey(Change, on_delete=models.CASCADE, db_constraint=True)
    build_id = models.CharField(max_length=255)
    builder_display_name = models.TextField()
    result = models.IntegerField(null=True, blank=True)
    url = models.TextField()
    state_string = models.TextField(blank=True, default='')
    ci_system = models.CharField(max_length=255, default='')
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return f'{self.ci_system}-Build-{self.build_id}'

    @classmethod
    def save_build(cls, commit_hash, build_id, builder_display_name, result, url, ci_system, pr_number, pr_project):
        build = CIBuild.get_existing_build(build_id)
        if build:
            # If the build data is already present in database, update it, e.g.: build complete event.
            return CIBuild.update_build(build, commit_hash, result, url)

        if not Change.is_existing_change_id(commit_hash):
            Change.save_change(change_id=commit_hash, pr_number=pr_number, pr_project=pr_project)
            _log.info(f'Received result for new commit. Saved commit {commit_hash} to database.')

        # Save the new build data, e.g.: build start event.
        CIBuild(change_id=commit_hash, build_id=build_id, builder_display_name=builder_display_name, result=result, url=url, ci_system=ci_system).save()
        _log.info(f'Saved build {build_id} in database for commit: {commit_hash}')
        return SUCCESS

    @classmethod
    def update_build(cls, cibuild, commit_hash, result, url):
        if str(cibuild.change_id) != str(commit_hash):
            _log.error(f'ERROR: build id: {cibuild.build_id}. Existing commit_hash {cibuild.change_id} of type {type(cibuild.change_id)} does not match with new commit_hash {commit_hash} of type {type(commit_hash)}. Ignoring new data.')
            return ERR_UNEXPECTED
        cibuild.result = result
        cibuild.url = url
        cibuild.save(update_fields=['result', 'url', 'modified'])
        _log.info(f'Updated CI build {cibuild.build_id} in database for commit: {commit_hash}')
        return SUCCESS

    @classmethod
    def get_existing_build(cls, build_id):
        try:
            return CIBuild.objects.get(build_id=build_id)
        except CIBuild.DoesNotExist:
            return None

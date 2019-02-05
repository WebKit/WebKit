# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
import ews.common.util as util

_log = logging.getLogger(__name__)


class BuilderMapping(models.Model):
    builder_id = models.IntegerField(primary_key=True)
    builder_name = models.TextField()
    display_name = models.TextField()
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return "{}: {}".format(self.builder_id, self.display_name)

    @classmethod
    def save_mapping(cls, builder_id, builder_name, display_name):
        if not BuilderMapping.is_valid_mapping(builder_id, builder_name, display_name):
            return ERR_UNEXPECTED

        mapping = BuilderMapping.get_existing_mapping(builder_id)
        if mapping:
            # If the mapping is updated, e.g.: display name changed.
            return BuilderMapping.update_mapping(mapping, builder_id, builder_name, display_name)

        BuilderMapping(builder_id, builder_name, display_name).save()
        _log.info('Saved mapping for builder_id: {}, name: {}, display_name: {}'.format(builder_id, builder_name, display_name))
        return SUCCESS

    @classmethod
    def update_mapping(cls, mapping, builder_id, builder_name, display_name):
        if mapping.builder_id != builder_id:
            _log.error('builder_id {} does not match with builder_id {}. Ignoring new data.'.format(mapping.builder_id, builder_id))
            return ERR_UNEXPECTED

        if mapping.builder_name == builder_name and mapping.display_name == display_name:
            _log.debug('Mapping already exist for builder_id: {}'.format(builder_id))
            return SUCCESS

        mapping.builder_name = builder_name
        mapping.display_name = display_name
        mapping.save(update_fields=['builder_name', 'display_name'])
        _log.info('Updated mapping for builder_id: {}, name: {}, display_name: {}'.format(builder_id, builder_name, display_name))
        return SUCCESS

    @classmethod
    def get_existing_mapping(cls, builder_id):
        try:
            return BuilderMapping.objects.get(builder_id=builder_id)
        except:
            return None

    @classmethod
    def is_valid_mapping(cls, builder_id, builder_name, display_name):
        if not util.is_valid_id(builder_id):
            return False
        return True

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


import json
import re

from buildbot.scheduler import AnyBranchScheduler, Periodic, Dependent, Triggerable, Nightly
from buildbot.worker import Worker
from buildbot.util import identifiers as buildbot_identifiers

from factories import *

BUILDER_NAME_LENGTH_LIMIT = 70
STEP_NAME_LENGTH_LIMIT = 50


def loadBuilderConfig(c):
    config = json.load(open('config.json'))
    passwords = json.load(open('passwords.json'))
    checkWorkersAndBuildersForConsistency(config['workers'], config['builders'])

    c['workers'] = [Worker(worker['name'], passwords.get(worker['name'], 'password')) for worker in config['workers']]
    c['builders'] = []
    for builder in config['builders']:
        builder['tags'] = getTagsForBuilder(builder)
        factory = globals()[builder['factory']]
        builder['factory'] = factory()
        del builder['platform']
        if 'configuration' in builder:
            del builder['configuration']
        c['builders'].append(builder)

    c['schedulers'] = []
    for scheduler in config['schedulers']:
        schedulerType = globals()[scheduler.pop('type')]
        # Python 2.6 can't handle unicode keys as keyword arguments:
        # http://bugs.python.org/issue2646.  Modern versions of json return
        # unicode strings from json.load, so we map all keys to str objects.
        scheduler = dict(map(lambda key_value_pair: (str(key_value_pair[0]), key_value_pair[1]), scheduler.items()))
        c['schedulers'].append(schedulerType(**scheduler))


def checkValidWorker(worker):
    if not worker:
        raise Exception('Worker is None or Empty.')

    if not worker.get('name'):
        raise Exception('Worker "{}" does not have name defined.'.format(worker))

    if not worker.get('platform'):
        raise Exception('Worker {} does not have platform defined.'.format(worker['name']))


def checkValidBuilder(builder):
    if not builder:
        raise Exception('Builder is None or Empty.')

    if not builder.get('name'):
        raise Exception('Builder "{}" does not have name defined.'.format(builder))

    if not buildbot_identifiers.ident_re.match(builder['name']):
        raise Exception('Builder name {} is not a valid buildbot identifier.'.format(builder['name']))

    if len(builder['name']) > BUILDER_NAME_LENGTH_LIMIT:
        raise Exception('Builder name {} is longer than maximum allowed by Buildbot ({} characters).'.format(builder['name'], BUILDER_NAME_LENGTH_LIMIT))

    if 'configuration' in builder and builder['configuration'] not in ['Debug', 'Production', 'Release']:
        raise Exception('Invalid configuration: {} for builder: {}'.format(builder.get('configuration'), builder.get('name')))

    if not builder.get('factory'):
        raise Exception('Builder {} does not have factory defined.'.format(builder['name']))

    if not builder.get('platform'):
        raise Exception('Builder {} does not have platform defined.'.format(builder['name']))


def checkWorkersAndBuildersForConsistency(workers, builders):
    def _find_worker_with_name(workers, worker_name):
        for worker in workers:
            if worker['name'] == worker_name:
                return worker
        return None

    for worker in workers:
        checkValidWorker(worker)

    for builder in builders:
        checkValidBuilder(builder)
        for worker_name in builder['workernames']:
            worker = _find_worker_with_name(workers, worker_name)
            if worker is None:
                raise Exception('Builder {} has worker {}, which is not defined in workers list!'.format(builder['name'], worker_name))

            if worker['platform'] != builder['platform']:
                raise Exception('Builder {0} is for platform {1}, but has worker {2} for platform {3}!'.format(
                    builder['name'], builder['platform'], worker['name'], worker['platform']))


def getBlackListedTags():
    """
    We maintain a blacklist of words which we do not want to display as tag in buildbot.
    We generate a list of tags by splitting the builder name. We do not want certain words as tag.
    For e.g. we don't want '11'as tag for builder iOS-11-Simulator-EWS
    """
    tags_blacklist = [str(i) for i in xrange(0, 20)]
    tags_blacklist.extend(['EWS', 'TryBot'])
    return tags_blacklist


def getValidTags(tags):
    return list(set(tags) - set(getBlackListedTags()))


def getTagsForBuilder(builder):
    keywords = filter(None, re.split('[, \-_:()]+', str(builder['name'])))
    return getValidTags(keywords)

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


import json
import os
import re

from buildbot.scheduler import AnyBranchScheduler, Periodic, Dependent, Triggerable, Nightly
from buildbot.schedulers.trysched import Try_Userpass
from buildbot.worker import Worker
from buildbot.util import identifiers as buildbot_identifiers

from factories import (APITestsFactory, BindingsFactory, BuildFactory, CommitQueueFactory, Factory, GTKBuildFactory,
                       GTKTestsFactory, JSCBuildFactory, JSCBuildAndTestsFactory, StyleFactory, TestFactory, tvOSBuildFactory,
                       WPEFactory, WebKitPerlFactory, WebKitPyFactory, WinCairoFactory, WindowsFactory, iOSBuildFactory, iOSEmbeddedBuildFactory, iOSTestsFactory,
                       macOSBuildFactory, macOSWK1Factory, macOSWK2Factory, ServicesFactory, WatchListFactory, watchOSBuildFactory)

BUILDER_NAME_LENGTH_LIMIT = 70
STEP_NAME_LENGTH_LIMIT = 50


def loadBuilderConfig(c, is_test_mode_enabled=False, master_prefix_path='./'):
    config = json.load(open(os.path.join(master_prefix_path, 'config.json')))
    use_localhost_worker = is_test_mode_enabled
    if is_test_mode_enabled:
        passwords = {}
    else:
        passwords = json.load(open(os.path.join(master_prefix_path, 'passwords.json')))

    checkWorkersAndBuildersForConsistency(config, config['workers'], config['builders'])
    checkValidSchedulers(config, config['schedulers'])

    c['workers'] = [Worker(worker['name'], passwords.get(worker['name'], 'password'), max_builds=worker.get('max_builds', 1)) for worker in config['workers']]
    if use_localhost_worker:
        c['workers'].append(Worker('local-worker', 'password', max_builds=1))

    c['builders'] = []
    for builder in config['builders']:
        builder['tags'] = getTagsForBuilder(builder)
        factory = globals()[builder['factory']]
        builder['description'] = builder.pop('shortname')
        if 'icon' in builder:
            del builder['icon']
        factorykwargs = {}
        for key in ['platform', 'configuration', 'architectures', 'triggers', 'remotes', 'additionalArguments', 'runTests']:
            value = builder.pop(key, None)
            if value:
                factorykwargs[key] = value

        builder['factory'] = factory(**factorykwargs)

        if use_localhost_worker:
            builder['workernames'].append('local-worker')

        c['builders'].append(builder)

    c['prioritizeBuilders'] = prioritizeBuilders
    c['schedulers'] = []
    for scheduler in config['schedulers']:
        schedulerClassName = scheduler.pop('type')
        schedulerClass = globals()[schedulerClassName]
        # Python 2.6 can't handle unicode keys as keyword arguments:
        # http://bugs.python.org/issue2646.  Modern versions of json return
        # unicode strings from json.load, so we map all keys to str objects.
        scheduler = dict(map(lambda key_value_pair: (str(key_value_pair[0]), key_value_pair[1]), scheduler.items()))
        if (schedulerClassName == 'Try_Userpass'):
            # FIXME: Read the credentials from local file on disk.
            scheduler['userpass'] = [(os.getenv('BUILDBOT_TRY_USERNAME', 'sampleuser'), os.getenv('BUILDBOT_TRY_PASSWORD', 'samplepass'))]
        c['schedulers'].append(schedulerClass(**scheduler))


def prioritizeBuilders(buildmaster, builders):
    # Prioritize builder queues over tester queues
    builders.sort(key=lambda b: 'build' in b.name.lower(), reverse=True)
    return builders


def checkValidWorker(worker):
    if not worker:
        raise Exception('Worker is None or Empty.')

    if not worker.get('name'):
        raise Exception('Worker "{}" does not have name defined.'.format(worker))

    if not worker.get('platform'):
        raise Exception('Worker {} does not have platform defined.'.format(worker['name']))


def checkValidBuilder(config, builder):
    if not builder:
        raise Exception('Builder is None or Empty.')

    if not builder.get('name'):
        raise Exception('Builder "{}" does not have name defined.'.format(builder))

    if not builder.get('shortname'):
        raise Exception('Builder "{}" does not have short name defined. This name is needed for EWS status bubbles.'.format(builder.get('name')))

    if not buildbot_identifiers.ident_re.match(builder['name']):
        raise Exception('Builder name {} is not a valid buildbot identifier.'.format(builder['name']))

    if len(builder['name']) > BUILDER_NAME_LENGTH_LIMIT:
        raise Exception('Builder name {} is longer than maximum allowed by Buildbot ({} characters).'.format(builder['name'], BUILDER_NAME_LENGTH_LIMIT))

    if 'configuration' in builder and builder['configuration'] not in ['debug', 'production', 'release']:
        raise Exception('Invalid configuration: {} for builder: {}'.format(builder.get('configuration'), builder.get('name')))

    if not builder.get('factory'):
        raise Exception('Builder {} does not have factory defined.'.format(builder['name']))

    if not builder.get('platform'):
        raise Exception('Builder {} does not have platform defined.'.format(builder['name']))

    for trigger in builder.get('triggers') or []:
        if not doesTriggerExist(config, trigger):
            raise Exception('Trigger: {} in builder {} does not exist in list of Trigerrable schedulers.'.format(trigger, builder['name']))


def checkValidSchedulers(config, schedulers):
    for scheduler in config.get('schedulers') or []:
        if scheduler.get('type') == 'Triggerable':
            if not isTriggerUsedByAnyBuilder(config, scheduler['name']):
                raise Exception('Trigger: {} is not used by any builder in config.json'.format(scheduler['name']))


def doesTriggerExist(config, trigger):
    for scheduler in config.get('schedulers') or []:
        if scheduler['name'] == trigger:
            return True
    return False


def isTriggerUsedByAnyBuilder(config, trigger):
    for builder in config.get('builders'):
        if trigger in (builder.get('triggers') or []):
            return True
    return False


def checkWorkersAndBuildersForConsistency(config, workers, builders):
    def _find_worker_with_name(workers, worker_name):
        result = None
        for worker in workers:
            if worker['name'] == worker_name:
                if not result:
                    result = worker
                else:
                    raise Exception('Duplicate worker entry found for {}.'.format(worker['name']))
        return result

    for worker in workers:
        checkValidWorker(worker)

    for builder in builders:
        checkValidBuilder(config, builder)
        for worker_name in builder['workernames']:
            worker = _find_worker_with_name(workers, worker_name)
            if worker is None:
                raise Exception('Builder {} has worker {}, which is not defined in workers list!'.format(builder['name'], worker_name))

            if worker['platform'] != builder['platform'] and worker['platform'] != '*' and builder['platform'] != '*':
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

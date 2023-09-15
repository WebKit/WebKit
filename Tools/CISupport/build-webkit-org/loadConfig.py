# Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
import operator
import os
import re

from buildbot.scheduler import AnyBranchScheduler, Triggerable, Nightly
from buildbot.schedulers.forcesched import BooleanParameter, CodebaseParameter, FixedParameter, ForceScheduler, StringParameter
from buildbot.schedulers.filter import ChangeFilter
from buildbot.process import buildstep, factory, properties
from buildbot.util import identifiers as buildbot_identifiers
from buildbot.worker import Worker

from .factories import *
from . import wkbuild

main_filter = ChangeFilter(branch=["main", None])

BUILDER_NAME_LENGTH_LIMIT = 70
STEP_NAME_LENGTH_LIMIT = 50


def pickLatestBuild(builder, requests):
    return max(requests, key=operator.attrgetter("submittedAt"))


def loadBuilderConfig(c, is_test_mode_enabled=False, master_prefix_path=None):
    if not master_prefix_path:
        master_prefix_path = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(master_prefix_path, 'config.json')) as config_json:
        config = json.load(config_json)
    if is_test_mode_enabled:
        passwords = {}
    else:
        passwords = json.load(open(os.path.join(master_prefix_path, 'passwords.json')))
    results_server_api_key = passwords.get('results-server-api-key')
    if results_server_api_key:
        os.environ['RESULTS_SERVER_API_KEY'] = results_server_api_key

    checkWorkersAndBuildersForConsistency(config, config['workers'], config['builders'])
    checkValidSchedulers(config, config['schedulers'])

    c['workers'] = [Worker(worker['name'], passwords.get(worker['name'], 'password'), max_builds=1) for worker in config['workers']]
    if is_test_mode_enabled:
        c['workers'].append(Worker('local-worker', 'password', max_builds=1))

    c['schedulers'] = []
    for scheduler in config['schedulers']:
        if "change_filter" in scheduler:
            scheduler["change_filter"] = globals()[scheduler["change_filter"]]
        schedulerClassName = scheduler.pop('type')
        schedulerClass = globals()[schedulerClassName]
        c['schedulers'].append(schedulerClass(**scheduler))

    # Setup force schedulers
    builderNames = [str(builder['name']) for builder in config['builders']]
    reason = StringParameter(name='reason', default='', size=40)
    properties = [StringParameter(name='user_provided_git_hash', label='git hash to build (optional)', required=False),
                  BooleanParameter(name='is_clean', label='Force Clean build')]
    # Disable default enabled input fields: revision, repository, project and branch
    codebases = [CodebaseParameter("",
                 revision=FixedParameter(name="revision", default=""),
                 repository=FixedParameter(name="repository", default=""),
                 project=FixedParameter(name="project", default=""),
                 branch=FixedParameter(name="branch", default=""))]
    forceScheduler = ForceScheduler(name='force', builderNames=builderNames, reason=reason, codebases=codebases, properties=properties)
    c['schedulers'].append(forceScheduler)

    c['builders'] = []
    for builder in config['builders']:
        builder['tags'] = getTagsForBuilder(builder)
        platform = builder['platform']
        factoryName = builder.pop('factory')
        factory = globals()[factoryName]
        factorykwargs = {}
        for key in ['platform', 'configuration', 'architectures', 'triggers', 'additionalArguments', 'device_model']:
            value = builder.pop(key, None)
            if value:
                factorykwargs[key] = value

        builder['factory'] = factory(**factorykwargs)

        if is_test_mode_enabled:
            builder['workernames'].append('local-worker')

        builder_name = builder['name']
        for step in builder["factory"].steps:
            step_name = step.buildStep().name
            if len(step_name) > STEP_NAME_LENGTH_LIMIT:
                raise Exception('step name "{}" is longer than maximum allowed by Buildbot ({} characters).'.format(step_name, STEP_NAME_LENGTH_LIMIT))
            if not buildbot_identifiers.ident_re.match(step_name):
                raise Exception('step name "{}" is not a valid buildbot identifier.'.format(step_name))

        if platform.startswith('mac'):
            category = 'AppleMac'
        elif platform.startswith('ios'):
            category = 'iOS'
        elif platform == 'win':
            category = 'AppleWin'
        elif platform.startswith('gtk'):
            category = 'GTK'
        elif platform.startswith('wpe'):
            category = 'WPE'
        elif platform == 'wincairo':
            category = 'WinCairo'
        elif platform.startswith('playstation'):
            category = 'PlayStation'
        else:
            category = 'misc'

        if (category in ('AppleMac', 'AppleWin', 'iOS')) and factoryName != 'BuildFactory':
            builder['nextBuild'] = pickLatestBuild

        c['builders'].append(builder)


class PlatformSpecificScheduler(AnyBranchScheduler):
    def __init__(self, platform, branch, **kwargs):
        self.platform = platform
        filter = ChangeFilter(branch=[branch, None], filter_fn=self.filter)
        AnyBranchScheduler.__init__(self, name=platform, change_filter=filter, **kwargs)

    def filter(self, change):
        return wkbuild.should_build(self.platform, change.files)


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
            if not isTriggerUsedByAnyBuilder(config, scheduler['name']) and 'build' not in scheduler['name'].lower():
                raise Exception('Trigger: {} is not used by any builder in config.json'.format(scheduler['name']))


def doesTriggerExist(config, trigger):
    for scheduler in config.get('schedulers') or []:
        if scheduler.get('name') == trigger:
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
                raise Exception('Builder "{0}" is for platform "{1}", but has worker "{2}" for platform "{3}"!'.format(
                    builder['name'], builder['platform'], worker['name'], worker['platform']))


def getInvalidTags():
    """
    We maintain a list of words which we do not want to display as tag in buildbot.
    We generate a list of tags by splitting the builder name. We do not want certain words as tag.
    For e.g. we don't want '11'as tag for builder iOS-11-Simulator-EWS
    """
    invalid_tags = [str(i) for i in range(0, 20)]
    invalid_tags.extend(['EWS', 'TryBot'])
    return invalid_tags


def getValidTags(tags):
    return list(set(tags) - set(getInvalidTags()))


def getTagsForBuilder(builder):
    keywords = re.split(r'[, \-_:()]+', str(builder['name']))
    return getValidTags(keywords)

# Copyright (C) 2017 Apple Inc. All rights reserved.
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

from buildbot.buildslave import BuildSlave
from buildbot.scheduler import AnyBranchScheduler, Triggerable, Nightly
from buildbot.schedulers.forcesched import FixedParameter, ForceScheduler, StringParameter, BooleanParameter
from buildbot.schedulers.filter import ChangeFilter
from buildbot.process import buildstep, factory, properties

from factories import *

import re
import json
import operator

from committer_auth import CommitterAuth
import make_passwords_json
import wkbuild

trunk_filter = ChangeFilter(branch=["trunk", None])


def pickLatestBuild(builder, requests):
    return max(requests, key=operator.attrgetter("submittedAt"))


def loadBuilderConfig(c, test_mode_is_enabled=False):
    # FIXME: These file handles are leaked.
    if test_mode_is_enabled:
        passwords = make_passwords_json.create_mock_slave_passwords_dict()
    else:
        passwords = json.load(open('passwords.json'))
    results_server_api_key = passwords.get('results-server-api-key')
    if results_server_api_key:
        os.environ['RESULTS_SERVER_API_KEY'] = results_server_api_key

    config = json.load(open('config.json'))
    c['slaves'] = [BuildSlave(slave['name'], passwords[slave['name']], max_builds=1) for slave in config['slaves']]

    c['schedulers'] = []
    for scheduler in config['schedulers']:
        if "change_filter" in scheduler:
            scheduler["change_filter"] = globals()[scheduler["change_filter"]]
        schedulerType = globals()[scheduler.pop('type')]
        # Python 2.6 can't handle unicode keys as keyword arguments:
        # http://bugs.python.org/issue2646.  Modern versions of json return
        # unicode strings from json.load, so we map all keys to str objects.
        scheduler = dict(map(lambda key_value_pair: (str(key_value_pair[0]), key_value_pair[1]), scheduler.items()))

        c['schedulers'].append(schedulerType(**scheduler))

    forceScheduler = ForceScheduler(
        name="force",
        builderNames=[str(builder['name']) for builder in config['builders']],
        reason=StringParameter(name="reason", default="", size=40),

        # Validate SVN revision: number or empty string
        revision=StringParameter(name="revision", default="", regex=re.compile(r'^(\d*)$')),

        # Disable default enabled input fields: branch, repository, project, additional properties
        branch=FixedParameter(name="branch"),
        repository=FixedParameter(name="repository"),
        project=FixedParameter(name="project"),
        properties=[BooleanParameter(name="is_clean", label="Force Clean build")]
    )
    c['schedulers'].append(forceScheduler)

    c['builders'] = []
    for builder in config['builders']:
        for slaveName in builder['slavenames']:
            for slave in config['slaves']:
                if slave['name'] != slaveName or slave['platform'] == '*':
                    continue

                if slave['platform'] != builder['platform']:
                    raise Exception, "Builder %r is for platform %r but has slave %r for platform %r!" % (builder['name'], builder['platform'], slave['name'], slave['platform'])

                break

        platform = builder['platform']

        builderType = builder.pop('type')
        factory = globals()["%sFactory" % builderType]
        factorykwargs = {}
        for key in "platform", "configuration", "architectures", "triggers", "additionalArguments", "SVNMirror", "device_model":
            value = builder.pop(key, None)
            if value:
                factorykwargs[key] = value

        builder["factory"] = factory(**factorykwargs)

        if platform.startswith('mac'):
            builder["category"] = 'AppleMac'
        elif platform.startswith('ios'):
            builder['category'] = 'iOS'
        elif platform == 'win':
            builder["category"] = 'AppleWin'
        elif platform.startswith('gtk'):
            builder["category"] = 'GTK'
        elif platform.startswith('wpe'):
            builder["category"] = 'WPE'
        elif platform == 'wincairo':
            builder["category"] = 'WinCairo'
        elif platform.startswith('playstation'):
            builder["category"] = 'PlayStation'
        else:
            builder["category"] = 'misc'

        if (builder['category'] in ('AppleMac', 'AppleWin', 'iOS')) and builderType != 'Build':
            builder['nextBuild'] = pickLatestBuild

        c['builders'].append(builder)


class PlatformSpecificScheduler(AnyBranchScheduler):
    def __init__(self, platform, branch, **kwargs):
        self.platform = platform
        filter = ChangeFilter(branch=[branch, None], filter_fn=self.filter)
        AnyBranchScheduler.__init__(self, name=platform, change_filter=filter, **kwargs)

    def filter(self, change):
        return wkbuild.should_build(self.platform, change.files)

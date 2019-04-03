# Copyright (C) 2019 Apple Inc. All rights reserved.
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

import datetime
import json
import os
import time

from buildbot.util import service
from twisted.internet import defer
from twisted.internet import reactor
from twisted.internet.defer import succeed
from twisted.python import log
from twisted.web.client import Agent
from twisted.web.http_headers import Headers
from twisted.web.iweb import IBodyProducer
from zope.interface import implements


class JSONProducer(object):
    """
    Perform JSON asynchronously as to not lock the buildbot main event loop
    """
    implements(IBodyProducer)

    def __init__(self, data):
        try:
            self.body = json.dumps(data, default=self.json_serialize_datetime)
        except TypeError:
            self.body = ''
        self.length = len(self.body)

    def startProducing(self, consumer):
        if self.body:
            consumer.write(self.body)
        return succeed(None)

    def pauseProducing(self):
        pass

    def stopProducing(self):
        pass

    def json_serialize_datetime(self, obj):
        """
        Serializing buildbot dates into UNIX epoch timestamps.
        """
        if isinstance(obj, datetime.datetime):
            return int(time.mktime(obj.timetuple()))

        raise TypeError("Type %s not serializable" % type(obj))


class Events(service.BuildbotService):

    EVENT_SERVER_ENDPOINT = 'https://ews.webkit.org/results/'

    def __init__(self, master_hostname, type_prefix='', name='Events'):
        """
        Initialize the Events Plugin. Sends data to event server on specific buildbot events.
        :param type_prefix: [optional] prefix we want to add to the 'type' field on the json we send
         to event server. (i.e. ews-build, where 'ews-' is the prefix.
        :return: Events Object
        """
        service.BuildbotService.__init__(self, name=name)

        if type_prefix and not type_prefix.endswith("-"):
            type_prefix += "-"
        self.type_prefix = type_prefix
        self.master_hostname = master_hostname

    def sendData(self, data):
        if os.getenv('EWS_API_KEY', None):
            data['EWS_API_KEY'] = os.getenv('EWS_API_KEY')
        agent = Agent(reactor)
        body = JSONProducer(data)

        agent.request('POST', self.EVENT_SERVER_ENDPOINT, Headers({'Content-Type': ['application/json']}), body)

    def getBuilderName(self, build):
        if not (build and 'properties' in build):
            return ''

        return build.get('properties').get('buildername')[0]

    def getPatchID(self, build):
        if not (build and 'properties' in build and 'patch_id' in build['properties']):
            return None

        return build.get('properties').get('patch_id')[0]

    @defer.inlineCallbacks
    def buildStarted(self, key, build):
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(build.get('buildid'))

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        builder_display_name = builder.get('description')

        data = {
            "type": self.type_prefix + "build",
            "status": "started",
            "hostname": self.master_hostname,
            "patch_id": self.getPatchID(build),
            "build_id": build.get('buildid'),
            "builder_id": build.get('builderid'),
            "number": build.get('number'),
            "result": build.get('results'),
            "started_at": build.get('started_at'),
            "complete_at": build.get('complete_at'),
            "state_string": build.get('state_string'),
            "builder_name": self.getBuilderName(build),
            "builder_display_name": builder_display_name,
        }

        self.sendData(data)

    @defer.inlineCallbacks
    def buildFinished(self, key, build):
        if not build.get('properties'):
            build['properties'] = yield self.master.db.builds.getBuildProperties(build.get('buildid'))
        if not build.get('steps'):
            build['steps'] = yield self.master.db.steps.getSteps(build.get('buildid'))

        builder = yield self.master.db.builders.getBuilder(build.get('builderid'))
        builder_display_name = builder.get('description')

        data = {
            "type": self.type_prefix + "build",
            "status": "finished",
            "hostname": self.master_hostname,
            "patch_id": self.getPatchID(build),
            "build_id": build.get('buildid'),
            "builder_id": build.get('builderid'),
            "number": build.get('number'),
            "result": build.get('results'),
            "started_at": build.get('started_at'),
            "complete_at": build.get('complete_at'),
            "state_string": build.get('state_string'),
            "builder_name": self.getBuilderName(build),
            "builder_display_name": builder_display_name,
            "steps": build.get('steps'),
        }

        self.sendData(data)

    def stepStarted(self, key, step):
        state_string = step.get('state_string')
        if state_string == 'pending':
            state_string = 'Running {}'.format(step.get('name'))

        data = {
            "type": self.type_prefix + "step",
            "status": "started",
            "hostname": self.master_hostname,
            "step_id": step.get('stepid'),
            "build_id": step.get('buildid'),
            "result": step.get('results'),
            "state_string": state_string,
            "started_at": step.get('started_at'),
            "complete_at": step.get('complete_at'),
        }

        self.sendData(data)

    def stepFinished(self, key, step):
        data = {
            "type": self.type_prefix + "step",
            "status": "finished",
            "hostname": self.master_hostname,
            "step_id": step.get('stepid'),
            "build_id": step.get('buildid'),
            "result": step.get('results'),
            "state_string": step.get('state_string'),
            "started_at": step.get('started_at'),
            "complete_at": step.get('complete_at'),
        }

        self.sendData(data)

    @defer.inlineCallbacks
    def startService(self):
        yield service.BuildbotService.startService(self)

        startConsuming = self.master.mq.startConsuming

        self._buildStartedConsumer = yield startConsuming(self.buildStarted, ('builds', None, 'new'))
        self._buildCompleteConsumer = yield startConsuming(self.buildFinished, ('builds', None, 'finished'))
        self._stepStartedConsumer = yield startConsuming(self.stepStarted, ('steps', None, 'started'))
        self._stepFinishedConsumer = yield startConsuming(self.stepFinished, ('steps', None, 'finished'))

    def stopService(self):
        self._buildStartedConsumer.stopConsuming()
        self._buildCompleteConsumer.stopConsuming()
        self._stepStartedConsumer.stopConsuming()
        self._stepFinishedConsumer.stopConsuming()

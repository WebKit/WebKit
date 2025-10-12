# Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from dnslib import CLASS, QTYPE, RR, RCODE
from dnslib.server import BaseResolver, DNSLogger, DNSServer
import logging
from threading import Thread

_log = logging.getLogger(__name__)


class Resolver(BaseResolver):
    def __init__(self, allowed_hosts=[]):
        super().__init__()
        self.hosts = list(map(lambda x: x if x.endswith(".") else x + ".", allowed_hosts))
        _log.info("Initializing Resolver with hosts: {}".format(self.hosts))

    def resolve(self, request, handler):
        question = request.q
        reply = request.reply()
        _log.debug("Received request for {}".format(question.qname))
        if question.qtype == QTYPE.A and (question.qname in self.hosts or question.qname.matchSuffix("localhost")):
            reply.add_answer(*RR.fromZone("{} 3600 A 127.0.0.1".format(question.qname)))
        else:
            reply.header.rcode = getattr(RCODE, "NXDOMAIN")
        return reply


if __name__ == "__main__":
    # This script is not intended to be run on its own, and should only be used for testing purposes.
    server = DNSServer(Resolver(["site.example"]), port=8053, address="127.0.0.1")
    server.start_thread()
    try:
        Thread.join()
    except Exception:
        pass
    finally:
        server.stop()

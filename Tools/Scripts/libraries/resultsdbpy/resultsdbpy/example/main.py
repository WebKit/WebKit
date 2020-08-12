# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import hashlib
import json

from example.environment import Environment, ModelFromEnvironment
from flask import abort, Flask, request
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.view.view_routes import ViewRoutes


environment = Environment()
print(f'Environment for web-app:\n{environment}')

model = ModelFromEnvironment(environment)
app = Flask(__name__)


api_routes = APIRoutes(model=model, import_name=__name__)
view_routes = ViewRoutes(
    title='Example Results Database',
    model=model, controller=api_routes, import_name=__name__,
)


@app.route('/__health', methods=('GET',))
def health():
    if not model.healthy(writable=True):
        abort(503, description='Health check failed, invalid database connections')
    return 'ok'


@app.errorhandler(401)
@app.errorhandler(404)
@app.errorhandler(405)
def handle_errors(error):
    if request.path.startswith('/api/'):
        return api_routes.error_response(error)
    return view_routes.error(error=error)


app.register_blueprint(api_routes)
app.register_blueprint(view_routes)


def main():
    app.run(host='0.0.0.0', port=environment.port)

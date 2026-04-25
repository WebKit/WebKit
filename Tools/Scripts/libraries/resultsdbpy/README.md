# resultsdbpy

Large projects (like [WebKit](https://webkit.org)) often have 10's of thousands of tests running on dozens of platforms. Making sense of results from theses tests is difficult. resultsdbpy aims to make visualizing, processing and storing those results easier.

## Requirements

For local testing and basic prototyping, nothing is required. All data can be managed in-memory. Note, however, that running the database in this mode will not save results to disk.

If leveraging Docker, Redis and Cassandra will be automatically installed and can be used to make results more persistent.

For production instances, the Cassandra and Redis instances should be hosted seperatly from the web-app.


## Usage

resultsdbpy requires fairly extensive configuration before being used. Below is an example of configuring resultsdbpy for testing and basic prototyping for Webkit, along with some comments explaining the environment setup:

```
import os

from fakeredis import FakeStrictRedis
from flask import Flask, request
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.model import Model
from resultsdbpy.model.repository import WebKitRepository
from resultsdbpy.view.view_routes import ViewRoutes

# By default, Cassandra forbids schema management
os.environ['CQLENG_ALLOW_SCHEMA_MANAGEMENT'] = '1'

# An in-memory Cassandra database for testing
cassandra=MockCassandraContext(
	nodes=['localhost'],
	keyspace='testing-kespace',
	create_keyspace=True,
)

model = Model(
	redis=FakeStrictRedis(),                 # An in-memory Redis database for testing
	cassandra=cassandra,
	repositories=[WebKitRepository()],       # This should be replaced with a class for your project's repository
	default_ttl_seconds=Model.TTL_WEEK * 4,  # Retain 4 weeks of results
	async_processing=False,                  # Processing asynchronously requires instantiating worker processes
)

app = Flask(__name__)
api_routes = APIRoutes(model=model, import_name=__name__)
view_routes = ViewRoutes(
    title='WebKit Results Database',
    model=model, controller=api_routes, import_name=__name__,
)


@app.route('/__health', methods=('GET',))
def health():
    return 'ok'


@app.errorhandler(404)
@app.errorhandler(405)
def handle_errors(error):
    if request.path.startswith('/api/'):
        return api_routes.error_response(error)
    return view_routes.error(error=error)


app.register_blueprint(api_routes)
app.register_blueprint(view_routes)


def main():
    app.run(host='0.0.0.0', port=5000)


if __name__ == '__main__':
    main()

```

import json
import sys
from urllib.request import Request, urlopen, HTTPBasicAuthHandler, HTTPDigestAuthHandler, HTTPPasswordMgr
from urllib.request import install_opener, build_opener


def submit_commits(commits, dashboard_url, worker_name, worker_password, status_to_accept=('OK',)):
    try:
        payload = json.dumps({
            'workerName': worker_name,
            'workerPassword': worker_password,
            'commits': commits,
        })
        request = Request(dashboard_url + '/api/report-commits')
        request.add_header('Content-Type', 'application/json')
        request.add_header('Content-Length', f'{len(payload)}')

        output = urlopen(request, payload.encode()).read()
        try:
            result = json.loads(output)
        except Exception as error:
            raise Exception(error, output)

        if result.get('status') not in status_to_accept:
            raise Exception(result)
        return result
    except Exception as error:
        sys.exit('Failed to submit commits: %s' % str(error))


def text_content(element):
    text = ''
    for child in element.childNodes:
        if child.nodeType == child.TEXT_NODE:
            text += child.data
        else:
            text += text_content(child)
    return text


HTTP_AUTH_HANDLERS = {
    'basic': HTTPBasicAuthHandler,
    'digest': HTTPDigestAuthHandler,
}


def setup_auth(server):
    auth = server.get('auth')
    if not auth:
        return

    password_manager = HTTPPasswordMgr()
    password_manager.add_password(realm=auth['realm'], uri=server['url'], user=auth['username'], passwd=auth['password'])
    auth_handler = HTTP_AUTH_HANDLERS[auth['type']](password_manager)
    install_opener(build_opener(auth_handler))


def load_server_config(json_path):
    with open(json_path) as server_config_json:
        server_config = json.load(server_config_json)
        server = server_config['server']
        server['url'] = server['scheme'] + '://' + server['host'] + ':' + str(server['port'])
        setup_auth(server)
        return server_config

import json
import sys
import urllib2


def submit_commits(commits, dashboard_url, slave_name, slave_password):
    try:
        payload = json.dumps({
            'slaveName': slave_name,
            'slavePassword': slave_password,
            'commits': commits,
        })
        request = urllib2.Request(dashboard_url + '/api/report-commits')
        request.add_header('Content-Type', 'application/json')
        request.add_header('Content-Length', len(payload))

        output = urllib2.urlopen(request, payload).read()
        try:
            result = json.loads(output)
        except Exception, error:
            raise Exception(error, output)

        if result.get('status') != 'OK':
            raise Exception(result)
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

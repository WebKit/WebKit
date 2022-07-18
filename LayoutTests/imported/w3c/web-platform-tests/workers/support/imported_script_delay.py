import random
import time

from wptserve.utils import isomorphic_encode


def main(request, response):
    time.sleep(random.uniform(0.2, 0.4))

    return [(b"Content-Type", b"application/javascript")], u"// Do nothing."

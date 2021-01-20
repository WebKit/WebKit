import argparse
from webkitpy.webdriver_plt.suites.suite import Suite
from webkitpy.webdriver_plt.liveplt import PageLoadTest

available_browsers = [
    "safari", "chrome", "firefox", "stp",
]


def parse_args():
    parser = argparse.ArgumentParser(description='Automate running PLT with live websites')
    parser.add_argument('-i', '--iterations', dest='iterations', type=int, default=3, help='Test the suite i times in the same browser session')
    parser.add_argument('-n', '--instances', dest='instances', type=int, default=3, help='Restart the browser n times for each suite')
    parser.add_argument('-w', '--wait', dest='wait', type=float, default=3.0, help='Wait time between pages')
    parser.add_argument('-b', '--browser', dest='browser', default='safari', choices=available_browsers)
    parser.add_argument('-s', '--suites', dest='suites', nargs='+', help='List one or more suites to run. If unspecified, defaults to running all suites')
    parser.add_argument('--width', dest='width', help='Set the inner window width')
    parser.add_argument('--height', dest='height', help='Set the inner window height')

    args = parser.parse_args()

    return args


def start(args):
    suites = make_suites(args.suites)
    size = (args.width, args.height)
    plt = PageLoadTest(args.iterations, args.instances, args.wait, args.browser, suites, size)
    plt.start()


def make_suites(suiteslist):
    available_suites = Suite.get_available_suites()
    suites = list()
    if suiteslist:
        for suitename in suiteslist:
            if suitename.lower() not in available_suites:
                print("Suite \"{}\" not found.".format(suitename))
                quit()
            suites.append(Suite(suitename.lower()))
    else:
        for suitename in available_suites:
            suites.append(Suite(suitename))
    return suites


def main():
    return start(parse_args())


if __name__ == '__main__':
    main()

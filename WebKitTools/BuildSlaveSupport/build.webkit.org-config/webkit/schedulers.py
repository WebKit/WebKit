from buildbot.scheduler import Scheduler, Periodic

def getSchedulers(builders):
    post_commit_builders = [b['name'] for b in builders if b['name'].startswith('post-commit-')] + ['page-layout-test-mac-os-x']
    post_commit_builders.sort()
    
    periodic_builders = [b['name'] for b in builders if b['name'].startswith('periodic-')]
    periodic_builders.sort()
    
    post_commit = Scheduler(name="post-commit", branch=None, treeStableTimer=90, builderNames=post_commit_builders)
    periodic = Periodic("periodic", periodic_builders, 6 * 60 * 60)

    return [post_commit, periodic]

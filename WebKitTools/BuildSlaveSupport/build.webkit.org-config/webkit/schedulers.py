from buildbot.scheduler import AnyBranchScheduler, Periodic

def getSchedulers(builders):
    builders = map(lambda builder: (builder['name'], builder['periodic']), builders)
    trunkBuilders = [name for name, periodic in builders if name.startswith('trunk-') and not periodic]
    trunkBuilders.sort()
    stableBuilders = [name for name, periodic in builders if name.startswith('stable-') and not periodic]
    stableBuilders.sort()
    periodicBuilders = [name for name, periodic in builders if periodic]
    periodicBuilders.sort()

    trunk = AnyBranchScheduler(name="trunk", branches=['trunk'], treeStableTimer=90, builderNames=trunkBuilders)
#    stable = AnyBranchScheduler(name="stable", branches=['branches/Safari-3-1-branch'], treeStableTimer=90, builderNames=stableBuilders)
    periodic = Periodic("periodic", periodicBuilders, 6 * 60 * 60)

    return [trunk, periodic]

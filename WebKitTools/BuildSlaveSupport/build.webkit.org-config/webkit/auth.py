from buildbot.buildslave import BuildSlave

def getSlaveAuthenticationDetails():
    def createBuildSlave((name, password)):
        return BuildSlave(name, password, max_builds=1)
    return map(createBuildSlave, _getSlaveAuthenticationDetails())

def _getSlaveAuthenticationDetails():
    return [("slave-name", "password")]

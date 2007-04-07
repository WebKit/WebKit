from buildbot.status import html, mail, words, client
from twisted.web.rewrite import RewriterResource

class MyWaterfall(html.Waterfall):
    def setup(self):
        html.Waterfall.setup(self)
        rootResource = self.site.resource
        def rewriteXSL(request):
            if request.postpath and (request.postpath[-1].endswith('.xsl') or
                                     (request.postpath[0] == 'results' and (request.postpath[-1].endswith('.css') or
                                                                            request.postpath[-1].endswith('.js'))
                                      )):
                request.postpath = ['results', request.postpath[-1]]
                request.path = '/' + '/'.join(request.prepath + request.postpath)
        self.site.resource = RewriterResource(rootResource, rewriteXSL)
        
waterfall = MyWaterfall(http_port=8010, css="buildbot.css", results_directory="/home/buildresults/results/", allowForce=False)

allBuildsEmail = mail.MailNotifier(fromaddr="buildbot@webkit.org",
                                    extraRecipients=["mark+webkit-builds@bdash.net.nz"],
                                    sendToInterestedUsers=False)
breakageEmail = mail.MailNotifier(fromaddr="buildbot@webkit.org",
                                  extraRecipients=["mark+webkit-builds@bdash.net.nz"],
                                  lookup=mail.Domain("webkit.org"),
                                  mode="breakage")

IRC = words.IRC(host="irc.freenode.net",
                nick="webkit-build",
                channels=["#webkit-build"],
                announceAllBuilds=True)

def getStatusListeners():
    return [waterfall, allBuildsEmail, breakageEmail, IRC]

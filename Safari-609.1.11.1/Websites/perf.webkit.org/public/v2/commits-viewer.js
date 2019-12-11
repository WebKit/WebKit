App.CommitsViewerComponent = Ember.Component.extend({
    repository: null,
    revisionInfo: null,
    commits: null,
    visible: true,
    commitsChanged: function ()
    {
        var revisionInfo = this.get('revisionInfo');

        var to = revisionInfo.get('currentRevision');
        var from = revisionInfo.get('previousRevision');
        var repository = this.get('repository');
        if (!from || !repository || !repository.get('hasReportedCommits'))
            return;

        var self = this;
        CommitLogs.fetchCommits(repository.get('id'), from, to).then(function (commits) {
            if (self.isDestroyed)
                return;
            self.set('commits', commits.map(function (commit) {
                return Ember.Object.create({
                    repository: repository,
                    revision: commit.revision,
                    url: repository.urlForRevision(commit.revision),
                    author: commit.authorName || commit.authorEmail,
                    message: commit.message ? commit.message.substr(0, 75) : null,
                });
            }));
        }, function () {
            if (!self.isDestroyed)
                self.set('commits', []);
        })
    }.observes('repository').observes('revisionInfo').on('init'),
    actions: {
        toggleVisibility: function ()
        {
            this.toggleProperty('visible');
        }
    }
});

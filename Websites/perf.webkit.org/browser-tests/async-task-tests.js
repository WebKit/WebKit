
describe('AsyncTask', () => {
    it('should have "AsyncTask" available for computing segmentation', async () => {
        const context = new BrowsingContext;
        await ChartTest.importChartScripts(context);
        expect(context.symbols.AsyncTask.isAvailable()).to.be(true);
    });
});
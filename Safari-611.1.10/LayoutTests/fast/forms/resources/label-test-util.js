// This file contains functions for testing the label element.

var withNoLabel = 0;
var withParentLabel = 1;
var withSiblingLabel = 2;
var withSibling2Label = 4;

// Populate dom tree and returns test data.
function setupLabelsTest(labelRelation, preHtml, postHtml)
{
    var html = '<div id="div1"></div><div id="div2">';
    if (preHtml)
        html += preHtml;

    var dataSet = createFormControlDataSet();

    for (var name in dataSet) {
        var data = dataSet[name];
        var id = data.name + '1';
        data.id = id;

        if (labelRelation & withParentLabel)
            html += '<label>';

        if (labelRelation & withSiblingLabel) {
            data.labelId = id + 'Label1';
            html += '<label for=' + id + ' id=' + data.labelId + '></label>';
        }

        if (labelRelation & withSibling2Label) {
            data.labelId2 = id + 'Label2';
            html += '<label for=' + id + ' id=' + data.labelId2 + '></label>';
        }

        if (data.inputType) {
            var typeName = data.inputType;
            html += '<input type=' + typeName + ' id=' + id + '>';
        } else {
            var tagName = data.tagName;
            html += '<' + tagName + ' id=' + id + '></' + tagName + '>';
        }

        if (labelRelation & withParentLabel)
            html += '</label>';
    }

    if (postHtml)
        html += postHtml;

    html += '</div>';

    var parent = document.createElement('div');
    parent.innerHTML = html;
    document.body.appendChild(parent);

    for (var name in dataSet) {
        var data = dataSet[name];
        data.element = document.getElementById(data.id);
    }

    return {
        dataSet: dataSet,

        getLabelableElementData: function (name)
        {
            var data = dataSet[name];
            return data && data.element.labels ? data : null;
        },

        outerElement: document.getElementById('div1'),
    };
}

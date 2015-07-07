function testAction()
{
    var a = document.createElement('a');
    a.setAttribute('href', 'javascript:alert("FAIL!");');
    document.body.appendChild(a);
    a.click();
}

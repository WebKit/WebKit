function testAction()
{
    var script = document.createElement('script');
    script.innerText = "alert('FAIL!');";
    document.body.appendChild(script);
}

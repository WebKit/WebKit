promise_test(async t => {
  const res = new Response(new FormData());
  const fd = await res.formData();
  assert_true(fd instanceof FormData);
  for (const pair of fd.entries()) { assert_true(false); }
}, 'Consume empty response.formData() as FormData');

promise_test(async t => {
  const req = new Request('about:blank', {
    method: 'POST',
    body: new FormData()
  });
  const fd = await req.formData();
  assert_true(fd instanceof FormData);
  for (const pair of fd.entries()) { assert_true(false); }
}, 'Consume empty request.formData() as FormData');

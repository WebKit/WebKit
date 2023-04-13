function checkResourcePriority(url, value, desciption) {
  assert_equals(internals.getResourcePriority(url), value, desciption);
}

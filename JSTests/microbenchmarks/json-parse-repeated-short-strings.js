//@ $skipModes << :lockdown if $buildType == "debug"

const json = '{"widgets":[{"id":"btn-1","kind":"button","size":"md","theme":"dark","events":["click","focus","blur"],"states":["idle","hover","active"]},{"id":"btn-2","kind":"button","size":"sm","theme":"light","events":["click","focus"],"states":["idle","hover","active","disabled"]},{"id":"inp-1","kind":"input","size":"md","theme":"dark","events":["input","change","focus","blur"],"states":["idle","focus","error"]},{"id":"sel-1","kind":"select","size":"lg","theme":"dark","events":["change","focus","blur"],"states":["idle","open","disabled"]},{"id":"mod-1","kind":"modal","size":"lg","theme":"light","events":["open","close"],"states":["hidden","shown"]},{"id":"tab-1","kind":"tabs","size":"md","theme":"dark","events":["change","click"],"states":["idle","active"]},{"id":"chk-1","kind":"checkbox","size":"sm","theme":"light","events":["change","click","focus"],"states":["off","on","mixed"]}],"themes":["dark","light","auto"],"sizes":["sm","md","lg"],"version":"2.1"}';

for (let i = 0; i < 1e4; ++i)
    JSON.parse(json);

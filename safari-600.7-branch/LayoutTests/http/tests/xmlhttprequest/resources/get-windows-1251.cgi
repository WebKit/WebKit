#!/usr/bin/perl -w
use utf8;
use Encode 'encode';

print "Content-type: text/plain; charset=windows-1251\n\n";
print encode("windows-1251", "Проверка");

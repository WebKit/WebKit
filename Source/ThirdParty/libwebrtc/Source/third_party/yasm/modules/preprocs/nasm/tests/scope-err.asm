%define a b
%error a
%scope
%define a c
%error a
%scope
%define a d
%error a
%endscope
%error a
%endscope
%error a
%endscope

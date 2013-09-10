description(
"This test checks for a crash when parsing NaN. You should see the text 'NaN' below."
);

debug(-parseFloat("NAN(ffffeeeeeff0f)"));

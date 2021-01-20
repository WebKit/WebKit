{};
{1};
{{}};
{{1}};
{1;{2}3}4;
{label:1};

label1:label2:true;

label:for(;;)continue;
label:for(;;){continue;}

label:while(1)continue;
label:while(1){continue;}

label:for(;;)break;
label:for(;;){break;}

label:while(1)break;
label:while(1){break;}

label:for(;;)continue label;
label:for(;;){continue label;}

label:while(1)continue label;
label:while(1){continue label;}

label:for(;;)break label;
label:for(;;){break label;}

label:while(1)break label;
label:while(1){break label;}

outer:while(true){if(false)continue;if(true)continue outer;break;}

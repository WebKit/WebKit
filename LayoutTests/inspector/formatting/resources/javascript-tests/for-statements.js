for(;;);
for(;;)1
for(1;;)1
for(;1;)1
for(;;1)1

for(;;){}
for(1;;){}
for(;1;){}
for(;;1){}

for(;;){1}
for(1;;){1}
for(;1;){1}
for(;;1){1}

for(;;);for(;;);
for(;;){}for(;;){}

for(;;){1}1
x

// for

for(1;2;3)1;
for(1;2;3){1};

for((1);(2);(3))1
for((1);(2);(3)){1}

for((1);(2);(3)) 1
for((1);(2);(3)) {1}

for ( 1 ; 2 ; 3 ) 1
for ( 1 ; 2 ; 3 ) {1}

for(var x=1;x<len;++x)1;
for(var x=1,len=10;x<len;++x)1;

for(x=1;x<len;++x){1}
for(x=1,len=10;x<len;++x){1}

for(var x=1;x<len;++x){1}
for(var x=1,len=10;x<len;++x){1}

for(var x=1;x<len;++x){1}
for(var x=1,len=10;x<len;++x){1}

for(1;2;3)for(4;5;6)for(7;8;9);
for(1;2;3){if(true){for(4;5;6)0}for(7;8;9);}

for(var x,y,z;!x&&y;++x,++y){}
for(var x=[],y=[1,2,3];true;false){}

// for..in

for(x in obj);
for(x in(obj));
for(var x in window);
for(var x in (window));
for(var{foo}in window);
for(var[x,y]in[1,2,3]);

// for..of

for(x of[1,2,3]);
for(x of([1,2,3]));
for(var x of window);
for(var x of (window));
for(var{foo}of window);
for(var[x,y]of[1,2,3]);

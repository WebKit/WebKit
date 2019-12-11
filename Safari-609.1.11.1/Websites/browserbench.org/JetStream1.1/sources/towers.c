#include <stdio.h>
#include <stdlib.h>

#define  nil		0
#define	 false		0
#define  true		1
#define  bubblebase	1.61f
#define  dnfbase 	3.5f
#define  permbase 	1.75f
#define  queensbase 1.83f
#define  towersbase 2.39f
#define  quickbase 	1.92f
#define  intmmbase 	1.46f
#define  treebase 	2.5f
#define  mmbase 	0.0f
#define  fpmmbase 	2.92f
#define  puzzlebase	0.5f
#define  fftbase 	0.0f
#define  fpfftbase 	4.44f
    /* Towers */
#define maxcells 	 18

    /* Intmm, Mm */
#define rowsize 	 40

    /* Puzzle */
#define size	 	 511
#define classmax 	 3
#define typemax 	 12
#define d 		     8

    /* Bubble, Quick */
#define sortelements 5000
#define srtelements  500

    /* fft */
#define fftsize 	 256 
#define fftsize2 	 129  
/*
type */
    /* Perm */
#define permrange     10

   /* tree */
struct node {
	struct node *left,*right;
	int val;
};

    /* Towers */ /*
    discsizrange = 1..maxcells; */
#define    stackrange	3
/*    cellcursor = 0..maxcells; */
struct    element {
	int discsize;
	int next;
};
/*    emsgtype = packed array[1..15] of char;
*/
    /* Intmm, Mm */ /*
    index = 1 .. rowsize;
    intmatrix = array [index,index] of integer;
    realmatrix = array [index,index] of real;
*/
    /* Puzzle */ /*
    piececlass = 0..classmax;
    piecetype = 0..typemax;
    position = 0..size;
*/
    /* Bubble, Quick */ /*
    listsize = 0..sortelements;
    sortarray = array [listsize] of integer;
*/
    /* FFT */
struct    complex { float rp, ip; } ;
/*
    carray = array [1..fftsize] of complex ;
    c2array = array [1..fftsize2] of complex ;
*/

float value, fixed, floated;

    /* global */
long    seed;  /* converted to long for 16 bit WR*/

    /* Perm */
int    permarray[permrange+1];
/* converted pctr to unsigned int for 16 bit WR*/
unsigned int    pctr;

    /* tree */
struct node *tree;

    /* Towers */
int	   stack[stackrange+1];
struct element    cellspace[maxcells+1];
int    freelist,  movesdone;

    /* Intmm, Mm */

int   ima[rowsize+1][rowsize+1], imb[rowsize+1][rowsize+1], imr[rowsize+1][rowsize+1];
float rma[rowsize+1][rowsize+1], rmb[rowsize+1][rowsize+1], rmr[rowsize+1][rowsize+1];

    /* Puzzle */
int	piececount[classmax+1],	class[typemax+1], piecemax[typemax+1];
int	puzzl[size+1], p[typemax+1][size+1], n, kount;

    /* Bubble, Quick */
int sortlist[sortelements+1], biggest, littlest, top;

    /* FFT */
struct complex    z[fftsize+1], w[fftsize+1], e[fftsize2+1];
float    zr, zi;

void Initrand () {
    seed = 74755L;   /* constant to long WR*/
}

int Rand () {
    seed = (seed * 1309L + 13849L) & 65535L;  /* constants to long WR*/
    return( (int)seed );     /* typecast back to int WR*/
}

 
    /*  Program to Solve the Towers of Hanoi */

void Error (char *emsg) 	{
	printf(" Error in Towers: %s\n",emsg);
}

void Makenull (int s) {
	stack[s]=0;
}

int Getelement () {
	int temp = 0;  /* force init of temp WR*/
	if ( freelist>0 ) {
	    temp = freelist;
	    freelist = cellspace[freelist].next;
	}
	else 
	    Error("out of space   ");
	return (temp);
}

void Push(int i, int s)	{
	int errorfound, localel;
	errorfound=false;
	if ( stack[s] > 0 )
		if ( cellspace[stack[s]].discsize<=i ) {
			errorfound=true;
			Error("disc size error");
		}
	if ( ! errorfound )	{
		localel=Getelement();
		cellspace[localel].next=stack[s];
		stack[s]=localel;
		cellspace[localel].discsize=i;
	}
}

void Init (int s, int n) {
	int discctr;
	Makenull(s);
	for ( discctr = n; discctr >= 1; discctr-- )
	    Push(discctr,s);
}

int Pop (int s)	{
	int temp, temp1;
	if ( stack[s] > 0 ) {
		temp1 = cellspace[stack[s]].discsize;
		temp = cellspace[stack[s]].next;
		cellspace[stack[s]].next=freelist;
		freelist=stack[s];
		stack[s]=temp;
		return (temp1);
	}
	else
		Error("nothing to pop ");
	return 0;
}

void Move (int s1, int s2) {
	Push(Pop(s1),s2);
	movesdone=movesdone+1;
}

void tower(int i, int j, int k) {
	int other;
	if ( k==1 ) Move(i,j);
	else {
	    other=6-i-j;
	    tower(i,other,k-1);
	    Move(i,j);
	    tower(other,j,k-1);
	}
}

void Towers ()    { /* Towers */
    int i;
    for ( i=1; i <= maxcells; i++ ) cellspace[i].next=i-1;
    freelist=maxcells;
    Init(1,14);
    Makenull(2);
    Makenull(3);
    movesdone=0;
    tower(1,2,14);
    if ( movesdone != 16383 ) printf (" Error in Towers.\n");
	 printf("%d\n", movesdone);
} /* Towers */

int main()
{
	int i;
	for (i = 0; i < 100; i++) Towers();
	return 0;
}

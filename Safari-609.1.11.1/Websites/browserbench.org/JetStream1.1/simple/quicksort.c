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


    /* Sorts an array using quicksort */
void Initarr() {
	int i; /* temp */
	long temp;  /* made temp a long for 16 bit WR*/
	Initrand();
	biggest = 0; littlest = 0;
	for ( i = 1; i <= sortelements; i++ ) {
	    temp = Rand();
	    /* converted constants to long in next stmt, typecast back to int WR*/
	    sortlist[i] = (int)(temp - (temp/100000L)*100000L - 50000L);
	    if ( sortlist[i] > biggest ) biggest = sortlist[i];
	    else if ( sortlist[i] < littlest ) littlest = sortlist[i];
	}
}

void Quicksort( int a[], int l, int r) {
	/* quicksort the array A from start to finish */
	int i,j,x,w;

	i=l; j=r;
	x=a[(l+r) / 2];
	do {
	    while ( a[i]<x ) i = i+1;
	    while ( x<a[j] ) j = j-1;
	    if ( i<=j ) {
			w = a[i];
			a[i] = a[j];
			a[j] = w;
			i = i+1;    j= j-1;
		}
	} while ( i<=j );
	if ( l <j ) Quicksort(a,l,j);
	if ( i<r ) Quicksort(a,i,r);
}


void Quick (int run) {
    Initarr();
    Quicksort(sortlist,1,sortelements);
    if ( (sortlist[1] != littlest) || (sortlist[sortelements] != biggest) )	printf ( " Error in Quick.\n");
	  printf("%d\n", sortlist[run + 1]);
}

int main()
{
	int i;
	for (i = 0; i < 100; i++) Quick(i);
	return 0;
}


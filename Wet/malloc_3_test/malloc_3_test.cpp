#include <unistd.h>
#include <stdio.h>
#include "my_stdlib.h"


int main(int argc, char* argv[])
{

    // void *p1 = smalloc(100); // head <--> 100
    // DEBUG_PrintList();
    // void *p2 = smalloc(95); // head <--> 95 <--> 100
    // DEBUG_PrintList();
    // void *p3 = smalloc(96); // head <--> 95 <--> 96 <--> 100
    // DEBUG_PrintList();
    // sfree(p1);              // head <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p4 = smalloc(80); // head <--> 80 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p5 = smalloc(85); // head <--> 80 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p6 = smalloc(76); // head <--> 76 <--> 80 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // sfree(p4);              // head <--> 76 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p7 = smalloc(21); // head <--> 21 <--> 76 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p8 = smalloc(73); // head <--> 21 <--> 73 <--> 76 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p9 = smalloc(58); // head <--> 21 <--> 58 <--> 73 <--> 76 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // sfree(p9);              // head <--> 21 <--> 73 <--> 76 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // void *p10 = smalloc(79); // head <--> 21 <--> 73 <--> 76 <--> 79 <--> 85 <--> 95 <--> 96
    // DEBUG_PrintList();
    // sfree(p2);
    // sfree(p3);
    // sfree(p5);
    // sfree(p6);
    // sfree(p7);
    // sfree(p8);
    // sfree(p10);


    // void *p11 = smalloc(250);
    // sfree(p11);
    // DEBUG_PrintList();
    // void *p12 = smalloc(30);
    // DEBUG_PrintList();
    // fflush(stdout);
    // sfree(p12);
    // DEBUG_PrintList();

    // void *p13 = smalloc(250);
    // DEBUG_PrintList();
    // sfree(p13);
    // DEBUG_PrintList();
    // void *p14 = smalloc(30);
    // DEBUG_PrintList();
    // void *p15 = smalloc(140);
    // DEBUG_PrintList();
    // //all of the allocated memory is i use now!
    // sfree(p14);
    // DEBUG_PrintList();
    // sfree(p15); // should unfy block with prev_block
    // DEBUG_PrintList();
    // void *p16 = smalloc(30);
    // DEBUG_PrintList();
    // void *p17 = smalloc(140);
    // DEBUG_PrintList();
    // sfree(p17);
    // DEBUG_PrintList();
    // sfree(p16); // should unfy block with next_block
    // DEBUG_PrintList();
    // void *p18 = smalloc(10);
    // DEBUG_PrintList();
    // void *p19 = smalloc(20);
    // DEBUG_PrintList();
    // void *p20 = smalloc(140);
    // DEBUG_PrintList();
    // sfree(p18);
    // DEBUG_PrintList();
    // sfree(p20);
    // DEBUG_PrintList(); 
    // sfree(p19); // should unfy block with both next_block and prev_block
    // DEBUG_PrintList();


    void *p21 = smalloc(50);
    DEBUG_PrintList();
    sfree(p21);             // all blocks are free
    DEBUG_PrintList();
    void *p22 = smalloc(70); // should increase the block size from 50 to 70
    DEBUG_PrintList();
    void *p23 = smalloc(20);  // should allocate new block of size 20
    DEBUG_PrintList();
    sfree(p23);               // wild is free now
    DEBUG_PrintList();
    void *p24 = smalloc(50);  // should use the wild_chunk and increase its size from 20 to 50
    DEBUG_PrintList();

    return 0;
}

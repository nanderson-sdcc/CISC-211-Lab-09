/*** asmMult.s   ***/
/* SOLUTION; used to test C test harness
 * VB 10/14/2023
 */
    
/* Tell the assembler to allow both 16b and 32b extended Thumb instructions */
.syntax unified

#include <xc.h>

/* Tell the assembler that what follows is in data memory    */
.data
.align
 
/* define and initialize global variables that C can access */

.global a_Multiplicand,b_Multiplier,rng_Error,a_Sign,b_Sign,prod_Is_Neg,a_Abs,b_Abs,init_Product,final_Product
.type a_Multiplicand,%gnu_unique_object
.type b_Multiplier,%gnu_unique_object
.type rng_Error,%gnu_unique_object
.type a_Sign,%gnu_unique_object
.type b_Sign,%gnu_unique_object
.type prod_Is_Neg,%gnu_unique_object
.type a_Abs,%gnu_unique_object
.type b_Abs,%gnu_unique_object
.type init_Product,%gnu_unique_object
.type final_Product,%gnu_unique_object

/* NOTE! These are only initialized ONCE, right before the program runs.
 * If you want these to be 0 every time asmMult gets called, you must set
 * them to 0 at the start of your code!
 */
a_Multiplicand:  .word     0  
b_Multiplier:    .word     0  
rng_Error:       .word     0 
a_Sign:          .word     0  
b_Sign:          .word     0 
prod_Is_Neg:     .word     0 
a_Abs:           .word     0  
b_Abs:           .word     0 
init_Product:    .word     0
final_Product:   .word     0

 /* Tell the assembler that what follows is in instruction memory    */
.text
.align

 /* Make the following functions globally visible */
.global asmUnpack, asmAbs, asmMult, asmFixSign, asmMain
.type asmUnpack,%function
.type asmAbs,%function
.type asmMult,%function
.type asmFixSign,%function
.type asmMain,%function

/* function: asmUnpack
 *    inputs:   r0: contains the packed value. 
 *                  MSB 16bits is signed multiplicand (a)
 *                  LSB 16bits is signed multiplier (b)
 *              r1: address where to store unpacked, 
 *                  sign-extended 32 bit a value
 *              r2: address where to store unpacked, 
 *                  sign-extended 32 bit b value
 *    outputs:  r0: No return value
 *              memory: 
 *                  1) store unpacked A value in location
 *                     specified by r1
 *                  2) store unpacked B value in location
 *                     specified by r2
 */
asmUnpack:   
    
    /*** STUDENTS: Place your asmUnpack code BELOW this line!!! **************/
    
    /* Function procedures */
    PUSH {R4-R11, LR}
    
    /* We need to unpack both values. Lets start with the MSB 16bits since it is simply a shift */
    MOV r4, r0
    ASR r4, r4, 16
    STR r4, [r1] @putting the value into memory

    /* Now we extract the value of B. This one is tricky - we do a bitmask and sign extension */
    MOV r5, r0 
    LDR r6, =0x0000FFFF @ we will use this for a bitmask
    AND r5, r5, r6 @ this ensures that we only look at the lower 16 bits of the original
    
    SXTH r5, r5 @this does a sign extension. I just discovered it
    STR r5, [r2]
    
    /* Function procedures */
    POP {R4-R11, LR}
    MOV PC, LR
    
    /*** STUDENTS: Place your asmUnpack code ABOVE this line!!! **************/


    /***************  END ---- asmUnpack  ************/

 
/* function: asmAbs
 *    inputs:   r0: contains signed value
 *              r1: address where to store absolute value
 *              r2: address where to store sign bit:
 *                  0 = "+", 1 = "-"
 *    outputs:  r0: Absolute value of r0 input. Same value
 *                  as stored to location given in r1
 *              memory: 
 *                  1) store absolute value in location
 *                     given by r1
 *                  2) store sign bit in location 
 *                     given by r2
 */
asmAbs:  
    /*** STUDENTS: Place your asmAbs code BELOW this line!!! **************/
    
    /* Function procedure */
    PUSH {R4-R11, LR}
    
    /* Getting the sign bit and storing it in r5 */
    MOV r4, 0x00007000
    AND r5, r0, r4
    LSR r5, r5, 15
    STR r5, [r2]

    /* Determine the absolute value by looking at the sign bit */
    CMP r5, 0
    BNE negative_sign @if it is negative, branch to a different procedure
    STR r0, [r1] @at this point, it is positive, so the absolute value is simply the value
    
    /* Function procedure */
    POP {R4-R11, LR}
    MOV PC, LR

negative_sign:
    /* Do the 2's complement procedure to get the absolute value of this int */
    NEG r0, r0
    ADD r0, r0, 1
    STR r0, [r1]

    /* Function procedure, since this the the end of a branch that is part of a function */
    POP {R4-R11, LR}
    MOV PC, LR
    
    /*** STUDENTS: Place your asmAbs code ABOVE this line!!! **************/


    /***************  END ---- asmAbs  ************/

 
/* function: asmMult
 *    inputs:   r0: contains abs value of multiplicand (a)
 *              r1: contains abs value of multiplier (b)
 *    outputs:  r0: initial product: r0 * r1
 */    
asmMult:   

    /*** STUDENTS: Place your asmMult code BELOW this line!!! **************/
    PUSH {r4-r11, LR}
    
    /* This is the same loop from the lecture. Running total is located in r4 */
mult:
    CMP r1, 0
    BEQ done

    TST r1, 1
    ADDNE r4, r4, r0

    LSL r0, r0, 1
    LSR r1, r1, 1
    B mult
    
done:
    MOV r0, r4 @store our product as a return value

    POP {r4-r11, LR}
    MOV PC, LR

    /*** STUDENTS: Place your asmMult code ABOVE this line!!! **************/
   
    /***************  END ---- asmMult  ************/


    
/* function: asmFixSign
 *    inputs:   r0: initial product: 
 *              (abs value of A) * (abs value of B)
 *              r1: sign bit of originally unpacked value
 *                  of A
 *              r2: sign bit of originally unpacked value
 *                  of B
 *    outputs:  r0: final product:
 *                  sign-corrected version of initial product
 */ 
asmFixSign:   
    
    /*** STUDENTS: Place your asmFixSign code BELOW this line!!! **************/
    PUSH {r4-r11, LR}

    CMP r1, r2
    NEGNE r0, r0 @if the signs are different, we convert the number using 2's compliment, since it is negative
    ADDNE r0, r0, 1

    POP {r4-r11, LR}
    MOV PC, LR
    
    /*** STUDENTS: Place your asmFixSign code ABOVE this line!!! **************/


    /***************  END ---- asmFixSign  ************/



    
/* function: asmMain
 *    inputs:   r0: contains packed value to be multiplied
 *                  using shift-and-add algorithm
 *           where: MSB 16bits is signed multiplicand (a)
 *                  LSB 16bits is signed multiplier (b)
 *    outputs:  r0: final product: sign-corrected product
 *                  of the two unpacked A and B input values
 *    NOTE TO STUDENTS: 
 *           To implement asmMain, follow the steps outlined
 *           in the comments in the body of the function
 *           definition below.
 */
asmMain:   
    
    /*** STUDENTS: Place your asmMain code BELOW this line!!! **************/
    
    /* Step 1:
     * call asmUnpack. Have it store the output values in 
     * a_Multiplicand and b_Multiplier.
     */
    BL asmUnpack

    /* Step 2a:
     * call asmAbs for the multiplicand (A). Have it store the
     * absolute value in a_Abs, and the sign in a_Sign.
     */
    BL asmAbs


    /* Step 2b:
     * call asmAbs for the multiplier (B). Have it store the
     * absolute value in b_Abs, and the sign in b_Sign.
     */


    /* Step 3:
     * call asmMult. Pass a_Abs as the multiplicand, 
     * and b_Abs as the multiplier.
     * asmMult returns the initial (positive) product in r0.
     * Store the value returned in r0 to mem location 
     * init_Product.
     */



    /* Step 4:
     * call asmFixSign. Pass in the initial product, and the
     * sign bits for the original a and b inputs. 
     * asmFixSign returns the final product with the correct
     * sign. 
     * Store the value returned in r0 to mem location 
     * final_Product.
     */



    /* Step 5:
     * END! Return to caller. Make sure of the following:
     * 1) Stack has been correctly managed.
     * 2) the final answer is stored in r0, so that the C call
     *    can access it.
     */



    
    /*** STUDENTS: Place your asmMain code ABOVE this line!!! **************/


    /***************  END ---- asmMain  ************/

 
    
    
.end   /* the assembler will ignore anything after this line. */

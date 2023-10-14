/*** asmMult.s   ***/
/* Tell the assembler to allow both 16b and 32b extended Thumb instructions */
.syntax unified

#include <xc.h>

/* Tell the assembler that what follows is in data memory    */
.data
.align
 



/* Tell the assembler that what follows is in instruction memory    */
.text
.align

 
/********************************************************************
function name: initConservedRegs
function description:
     output = initConservedRegs ()
     
where:
     output: 
     
     function description: The C call ..........
     
     notes:
        None
          
********************************************************************/    

.global initConservedRegs
.type initConservedRegs,%function
 /* initConservedRegs loads r4-r11 with test data.
  * The goal is to make sure that the data is still there after calling the
  * student's functions 
  * input: r0 contains a pointer to data to be used to init r4-r11. 
  *        MUST point to 8 valid word32's 
  * output: none (void) 
  */
initConservedRegs:
    LDMIA r0!,{r4-r11} /*load 8 registers with test data */
    BX LR /* return */
 
.global extractConservedRegs
.type extractConservedRegs,%function
/* extractConservedRegs: extract r4-r11 to specified mem location
 * input: r0 contains the address to where r4-r11 will be extracted.
 *        MUST point to valid data mem space for 8 word32's
 * output: non (void) 
 */
extractConservedRegs:   

    /* save the caller's registers, as required by the ARM calling convention */
    push {r4-r11,LR}
    
    /* Use STMIA to copy r4-r11 to specified mem location */
    STMIA r0!,{r4-r11}
    
    BX LR /* return */

/* Test data stored below */    
.data
.align

.if 0

.global test_reg_arr,extracted_regs
.type test_reg_arr,%gnu_unique_object
.type extracted_regs,%gnu_unique_object
/* the following data is used to load r4-r11 prior to a call to student's
 * code. It is also used as the basis for comparison to make sure those
 * registers still contain the same value upon return.
 */
test_reg_arr: .word  0xCAFEF00D, 0x7250D00D, 0xD000000D, 0x1000000D,0x42424242, 0xCAAAAAAA, 0xC0DE0042, 0x08675309
extracted_regs: .space 4*8, 0 /* make space to extact 8 registers r4-r11 */

.endif
 
.end  /* The assembler will not process anything after this directive!!! */
           





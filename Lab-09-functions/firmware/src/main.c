/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project. It is intended to
    be used as the starting point for CISC-211 Curiosity Nano Board
    programming projects. After initializing the hardware, it will
    go into a 0.5s loop that calls an assembly function specified in a separate
    .s file. It will print the iteration number and the result of the assembly 
    function call to the serial port.
    As an added bonus, it will toggle the LED on each iteration
    to provide feedback that the code is actually running.
  
    NOTE: PC serial port MUST be set to 115200 rate.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/


// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdio.h>
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include <malloc.h>
#include <inttypes.h>   // required to print out pointers using PRIXPTR macro
#include "definitions.h"                // SYS function prototypes

/* RTC Time period match values for input clock of 1 KHz */
#define PERIOD_500MS                            512
#define PERIOD_1S                               1024
#define PERIOD_2S                               2048
#define PERIOD_4S                               4096

#define MAX_PRINT_LEN 1000

static volatile bool isRTCExpired = false;
static volatile bool changeTempSamplingRate = false;
static volatile bool isUSARTTxComplete = true;
static uint8_t uartTxBuffer[MAX_PRINT_LEN] = {0};

#if 0
// Test cases for testing func that adds 3 nums and returns the results
// AND sets bits in global variables.
static int32_t depositArray[] = {  0x80000001, 0, 0x80000001,
                                   5,5,6};
static int32_t withdrawalArray[] = {  0x80000001, 0x80000001, 0,
                                      -2, -6, 5};
static int32_t balanceArray[] = {  0, 0x80000001, 0x80000001,
                                  -3, -9, 4};
static int32_t problemArray[] = {1,1,1,0,0,0};
#endif

#define NUM_CONSERVED_REGS 8 // r4-r11

/* test values to be stored in r4-r11 prior to calling student's funcs */
static uint32_t conservedRegInitValues[NUM_CONSERVED_REGS] = {
    0xCAFEF00D, 0x7250D00D, 0xD000000D, 0x1000000D,
    0x42424242, 0xCAAAAAAA, 0xC0DE0042, 0x08675309
};

/* Used to store the values extracted from r4-r11 for comparison to original */
static uint32_t extractedRegValues[NUM_CONSERVED_REGS];

/* asm funcs used to init and test values stored in registers,
 * before and after calling student's functions 
 */
extern void initConservedRegs(uint32_t* ptr);
extern void extractConservedRegs(uint32_t* ptr);

/* returns the number of mismatches */
static uint32_t checkConservedRegs(uint32_t* p1, uint32_t* p2, int32_t numRegs)
{
    int32_t retVal = 0;
    for (int i = 0; i < NUM_CONSERVED_REGS; ++i)
    {
        if(*p1++ != *p2++)
        {
            ++retVal;
        }
    }
    return retVal;
};


// the following array defines pairs of {balance, transaction} values
// tc stands for test case
static int32_t tc[] = {
    0xFFFC0003,  // -,+
    0,  // 0,0
    0x00000005,  // 0,+
    0x0000FFFC,  // 0,-
    0xFFFD0000,  // -,0
    0x00020000,  // +,0
    0x80008000,  // -,-
    0xFFF3FFE0,  // -,-
    0x7FF38001,  // +,-
    0x7FF17FF2   // +,+
};

static char * pass = "PASS";
static char * fail = "FAIL";

// VB COMMENT:
// The ARM calling convention permits the use of up to 4 registers, r0-r3
// to pass data into a function. Only one value can be returned to the 
// C caller. The assembly language routine stores the return value
// in r0. The C compiler will automatically use it as the function's return
// value.
//
/* These functions will be implemented by students in asmMult.s */
/* asmUnpack: return 0 if unpacked values within signed 16b range, 
 * non-zero otherwise */
extern int32_t asmUnpack(uint32_t packedValue, int32_t* a, int32_t* b);
/* asmAbs: return abs value. Also store abs value at location absOut,
 * and sign bit at location signBit. Must b 0 for +, 1 for negative */
extern int32_t asmAbs(int32_t input, int32_t *absOut, int32_t *signBit);
/* return product of two positive integers guaranteed to be <= 2^16 */
extern int32_t asmMult(int32_t a, int32_t b);
/* return corrected product based on signs of two original input values */
extern int32_t asmFixSign(int32_t initProduct, int32_t signBitA, int32_t signBitB);

struct operand {
    int32_t inputValue;
    int32_t absValue; /* absolute value of input value */
    int32_t signBit; /* sign bit of input value, 0 = +, 1 = - */
} ;

static struct operand a,b;

extern int32_t a_Multiplicand;
extern int32_t b_Multiplier;
extern int32_t rng_Error;
extern int32_t a_Sign;
extern int32_t b_Sign;
extern int32_t prod_Is_Neg;
extern int32_t a_Abs;
extern int32_t b_Abs;
extern int32_t init_Product;
extern int32_t final_Product;


// set this to 0 if using the simulator. BUT note that the simulator
// does NOT support the UART, so there's no way to print output.
#define USING_HW 1

#if USING_HW
static void rtcEventHandler (RTC_TIMER32_INT_MASK intCause, uintptr_t context)
{
    if (intCause & RTC_MODE0_INTENSET_CMP0_Msk)
    {            
        isRTCExpired    = true;
    }
}
static void usartDmaChannelHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    {
        isUSARTTxComplete = true;
    }
}
#endif

static void check(int32_t in1, 
        int32_t in2, 
        int32_t *goodCount, 
        int32_t *badCount,
        char **pfString )
{
    if (in1 == in2)
    {
        *goodCount += 1;
        *pfString = pass;
    }
    else
    {
        *badCount += 1;
        *pfString = fail;        
    }
    return;
}

// print the mem addresses of the global vars at startup
// this is to help the students debug their code
static void printGlobalAddresses(void)
{
    // build the string to be sent out over the serial lines
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= GLOBAL VARIABLES MEMORY ADDRESS LIST\r\n"
            "global variable \"a_Multiplicand\" stored at address: 0x%" PRIXPTR "\r\n"
            "global variable \"b_Multiplier\" stored at address:   0x%" PRIXPTR "\r\n"
            "global variable \"rng_Error\" stored at address:      0x%" PRIXPTR "\r\n"
            "global variable \"a_Sign\" stored at address:         0x%" PRIXPTR "\r\n"
            "global variable \"b_Sign\" stored at address:         0x%" PRIXPTR "\r\n"
            "global variable \"prod_Is_Neg\" stored at address:    0x%" PRIXPTR "\r\n"
            "global variable \"a_Abs\" stored at address:          0x%" PRIXPTR "\r\n"
            "global variable \"b_Abs\" stored at address:          0x%" PRIXPTR "\r\n"
            "global variable \"init_Product\" stored at address:   0x%" PRIXPTR "\r\n"
            "global variable \"final_Product\" stored at address:  0x%" PRIXPTR "\r\n"
            "========= END -- GLOBAL VARIABLES MEMORY ADDRESS LIST\r\n"
            "\r\n",
            (uintptr_t)(&a_Multiplicand), 
            (uintptr_t)(&b_Multiplier), 
            (uintptr_t)(&rng_Error), 
            (uintptr_t)(&a_Sign), 
            (uintptr_t)(&b_Sign), 
            (uintptr_t)(&prod_Is_Neg), 
            (uintptr_t)(&a_Abs), 
            (uintptr_t)(&b_Abs),
            (uintptr_t)(&init_Product), 
            (uintptr_t)(&final_Product)
            ); 
    isRTCExpired = false;
    isUSARTTxComplete = false;

#if USING_HW 
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
        (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
        strlen((const char*)uartTxBuffer));
    // spin here, waiting for timer and UART to complete
    while (isUSARTTxComplete == false); // wait for print to finish
    /* reset it for the next print */
    isUSARTTxComplete = false;
#endif
}


// return failure count. A return value of 0 means everything passed.
static int testResult(int testNum, 
                      int32_t result, 
                      int32_t *passCount,
                      int32_t *failCount)
{
    // for this lab, each test case corresponds to a single pass or fail
    // But for future labs, one test case may have multiple pass/fail criteria
    // So I'm setting it up this way so it'll work for future labs, too --VB
    *failCount = 0;
    *passCount = 0;
    char *aCheck = "OOPS";
    char *bCheck = "OOPS";
    char *rngCheck = "OOPS";
    char *aSignCheck = "OOPS";
    char *bSignCheck = "OOPS";
    char *prodSignCheck = "OOPS";
    char *aAbsCheck = "OOPS";
    char *bAbsCheck = "OOPS";
    char *initProdCheck = "OOPS";
    char *finalProdCheck = "OOPS";
    char *resultCheck = "OOPS";
    static bool firstTime = true;
    int32_t myA = tc[testNum];
    int32_t myB = tc[testNum];
    int32_t myErrCheck = 0;
    if( (myA > 32767) || (myA < -32768) || (myB > 32767) || (myB < -32768) )
    {
        myErrCheck = 1;
    }
    int32_t myAbsA = abs(myA);
    int32_t myAbsB = abs(myB);
    int32_t myFinalProd = myA * myB;
    int32_t myInitProd = abs(myFinalProd);
    int32_t mySignBitA = 0;
    int32_t mySignBitB = 0;
    if (myA < 0)
    {
        mySignBitA = 1;
    }
    if (myB < 0)
    {
        mySignBitB = 1;
    }
    
    int32_t myProdSign = 0;
    if (((myA < 0) && (myB > 0)) || ((myA > 0) && (myB < 0)))
    {
        myProdSign = 1;
    }
    
    check(myA, a_Multiplicand, passCount, failCount, &aCheck);
    check(myB, b_Multiplier, passCount, failCount, &bCheck);
    check(myErrCheck, rng_Error, passCount, failCount, &rngCheck);
    check(mySignBitA, a_Sign, passCount, failCount, &aSignCheck);
    check(mySignBitB, b_Sign, passCount, failCount, &bSignCheck);
    check(myProdSign, prod_Is_Neg, passCount, failCount, &prodSignCheck);
    check(myAbsA, a_Abs, passCount, failCount, &aAbsCheck);
    check(myAbsB, b_Abs, passCount, failCount, &bAbsCheck);
    check(myInitProd, init_Product, passCount, failCount, &initProdCheck);
    check(myFinalProd, final_Product, passCount, failCount, &finalProdCheck);
    check(myFinalProd, result, passCount, failCount, &resultCheck);
    
    /* Do first time stuff here, if needed!!!  */
    if (firstTime == true)
    {
        /* Do first time stuff here, if needed!!!  */
        
        firstTime = false; // don't check the strings for subsequent test cases
    }
           
    // build the string to be sent out over the serial lines
    snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
            "========= Test Number: %d =========\r\n"
            "test case INPUT: multiplier (a):   %11ld\r\n"
            "test case INPUT: multiplicand (b): %11ld\r\n"
            "a check p/f:           %s\r\n"
            "b check p/f:           %s\r\n"
            "input range check p/f: %s\r\n"
            "sign bit a check p/f:  %s\r\n"
            "sign bit b check p/f:  %s\r\n"
            "prod sign check p/f:   %s\r\n"
            "abs a check p/f:       %s\r\n"
            "abs b check p/f:       %s\r\n"
            "initial product p/f:   %s\r\n"
            "final product p/f:     %s\r\n"
            "returned result p/f:   %s\r\n"
            "debug values        expected        actual\r\n"
            "a_Multiplicand:..%11ld   %11ld\r\n"
            "b_Multiplier:....%11ld   %11ld\r\n"
            "error check:.....%11ld   %11ld\r\n"
            "a_Sign:..........%11ld   %11ld\r\n"
            "b_Sign:..........%11ld   %11ld\r\n"
            "prod_Is_Neg:.....%11ld   %11ld\r\n"
            "a_Abs:...........%11ld   %11ld\r\n"
            "b_Abs:...........%11ld   %11ld\r\n"
            "init_Product:....%11ld   %11ld\r\n"
            "final_Product:...%11ld   %11ld\r\n"
            "returned value:..%11ld   %11ld\r\n",
            testNum,
            myA, 
            myB,
            aCheck,
            bCheck,
            rngCheck,
            aSignCheck,
            bSignCheck,
            prodSignCheck,
            aAbsCheck,
            bAbsCheck,
            initProdCheck,
            finalProdCheck,
            resultCheck,
            myA, a_Multiplicand,
            myB, b_Multiplier,
            myErrCheck,rng_Error,
            mySignBitA, a_Sign,
            mySignBitB, b_Sign,
            myProdSign, prod_Is_Neg,
            myAbsA, a_Abs,
            myAbsB, b_Abs,
            myInitProd, init_Product,
            myFinalProd, final_Product,
            myFinalProd, result
            );

#if USING_HW 
    // send the string over the serial bus using the UART
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
        (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
        strlen((const char*)uartTxBuffer));

    // spin here until the UART has completed transmission
    //while  (false == isUSARTTxComplete ); 
    while (isUSARTTxComplete == false);
    isUSARTTxComplete = false;
#endif

    return *failCount;
    
}



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
int main ( void )
{
    
 
#if USING_HW
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, usartDmaChannelHandler, 0);
    RTC_Timer32CallbackRegister(rtcEventHandler, 0);
    RTC_Timer32Compare0Set(PERIOD_500MS);
    RTC_Timer32CounterSet(0);
    RTC_Timer32Start();
#else // using the simulator
    isRTCExpired = true;
    isUSARTTxComplete = true;
#endif //SIMULATOR
    
    printGlobalAddresses();

    // initialize all the variables
    int32_t passCount = 0;
    int32_t failCount = 0;
    int32_t totalPassCount = 0;
    int32_t totalFailCount = 0;
    int32_t totalTests = 0;
    // int32_t x1 = sizeof(tc);
    // int32_t x2 = sizeof(tc[0]);
    uint32_t numTestCases = sizeof(tc)/sizeof(tc[0]);
    
    // Loop forever
    while ( true )
    {
        // Do the tests
        for (int testCase = 0; testCase < numTestCases; ++testCase)
        {
            // Toggle the LED to show we're running a new test case
            LED0_Toggle();

            // reset the state variables for the timer and serial port funcs
            isRTCExpired = false;
            isUSARTTxComplete = false;
            
            int32_t packedValue = tc[testCase];  // multiplicand
            // !!!! THIS IS WHERE YOUR ASSEMBLY LANGUAGE PROGRAM GETS CALLED!!!!
            // Call our assembly function defined in file asmMult.s
            initConservedRegs(conservedRegInitValues);            // set the input values 
            int32_t err1 = asmUnpack(packedValue, &a.inputValue, &b.inputValue);
            int32_t numFailedRegs = checkConservedRegs(conservedRegInitValues, 
                                                       extractedRegValues, 
                                                       NUM_CONSERVED_REGS);
/* asmAbs: return abs value. Also store abs value at location absOut,
 * and sign bit at location signBit. Must b 0 for +, 1 for negative */
            initConservedRegs(conservedRegInitValues);            // set the input values 
            int32_t absValA = asmAbs(a.inputValue, &a.absValue, &a.signBit);
            numFailedRegs = checkConservedRegs(conservedRegInitValues, 
                                                       extractedRegValues, 
                                                       NUM_CONSERVED_REGS);
            initConservedRegs(conservedRegInitValues);            // set the input values 
            int32_t absValB = asmAbs(b.inputValue, &b.absValue, &b.signBit);
            numFailedRegs = checkConservedRegs(conservedRegInitValues, 
                                                       extractedRegValues, 
                                                       NUM_CONSERVED_REGS);
/* return product of two positive integers guaranteed to be <= 2^16 */
            initConservedRegs(conservedRegInitValues);            // set the input values 
            int32_t initProd = asmMult(a.absValue, b.absValue);
            numFailedRegs = checkConservedRegs(conservedRegInitValues, 
                                                       extractedRegValues, 
                                                       NUM_CONSERVED_REGS);
/* return corrected product based on signs of two original input values */
            initConservedRegs(conservedRegInitValues);            // set the input values 
            int32_t finalProduct = asmFixSign(initProd, a.signBit, b.signBit);
            numFailedRegs = checkConservedRegs(conservedRegInitValues, 
                                                       extractedRegValues, 
                                                       NUM_CONSERVED_REGS);
            
            // test the result and see if it passed
            failCount = testResult(testCase,finalProduct,
                                   &passCount,&failCount);
            totalPassCount = totalPassCount + passCount;
            totalFailCount = totalFailCount + failCount;
            totalTests = totalPassCount + totalFailCount;

#if USING_HW
            
            // print summary of tests executed so far
            isUSARTTxComplete = false;
            snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
                    "========= In-progress test summary:\r\n"
                    "%ld of %ld tests passed so far...\r\n"
                    "%ld, %ld, %ld, %ld, %ld, %ld for debugging\r\n"
                    "\r\n",
                    totalPassCount, totalTests,absValA,absValB,numFailedRegs,err1,initProd,finalProduct); 

            DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
                (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
                strlen((const char*)uartTxBuffer));

            // spin here until the UART has completed transmission
            // and the LED toggle timer has expired. This allows
            // the test cases to be spread out in time.
            while ((isRTCExpired == false) ||
                   (isUSARTTxComplete == false));
            
#endif

        } // for each test case
        
        // When all test cases are complete, print the pass/fail statistics
        // Keep looping so that students can see code is still running.
        // We do this in case there are very few tests and they don't have the
        // terminal hooked up in time.
        uint32_t idleCount = 1;
        // uint32_t totalTests = totalPassCount + totalFailCount;
        bool firstTime = true;
        uint32_t t1 = 0xAABBCCDD;
        uint32_t t2 = 0x66778899;
        while(true)      // post-test forever loop
        {
            isRTCExpired = false;
            isUSARTTxComplete = false;
            snprintf((char*)uartTxBuffer, MAX_PRINT_LEN,
                    "========= TESTS COMPLETE: Post-test Idle Cycle Number: %ld\r\n"
                    "Summary of tests: %ld of %ld tests passed\r\n"
                    "%ld, %ld (just doing stack experiments)\r\n"
                    "\r\n",
                    idleCount, totalPassCount, totalTests,t1,t2); 

#if USING_HW 
            DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
                (const void *)&(SERCOM5_REGS->USART_INT.SERCOM_DATA), \
                strlen((const char*)uartTxBuffer));
            // spin here, waiting for timer and UART to complete
            LED0_Toggle();
            ++idleCount;

            while ((isRTCExpired == false) ||
                   (isUSARTTxComplete == false));

            // slow down the blink rate after the tests have been executed
            if (firstTime == true)
            {
                firstTime = false; // only execute this section once
                RTC_Timer32Compare0Set(PERIOD_4S); // set blink period to 4sec
                RTC_Timer32CounterSet(0); // reset timer to start at 0
            }
#endif
        } // end - post-test forever loop
        
        // Should never get here...
        break;
    } // while ...
            
    /* Execution should not come here during normal operation */
    return ( EXIT_FAILURE );
}
/*******************************************************************************
 End of File
*/


/*
 * File:   SWI_INLINE.h
 * Author: xxxajk@gmail.com
 *
 * Created on December 5, 2014, 9:40 AM
 *
 * This is the actual library.
 * There are no 'c' or 'cpp' files.
 *
 */

#ifndef SWI_INLINE_H
#define	SWI_INLINE_H

#if defined(__arm__)

#ifndef SWI_MAXIMUM_ALLOWED
#define SWI_MAXIMUM_ALLOWED 4
#endif

#if defined(__USE_CMSIS_VECTORS__)

extern "C" {
        void (*_VectorsRam[VECTORTABLE_SIZE])(void)__attribute__((aligned(VECTORTABLE_ALIGNMENT)));
}
#else

__attribute__((always_inline)) static inline void __DSB(void) {
        __asm__ volatile ("dsb");
}

#endif

#ifndef interruptsStatus
#define interruptsStatus() __interruptsStatus()
static inline unsigned char __interruptsStatus(void) __attribute__((always_inline, unused));

static inline unsigned char __interruptsStatus(void) {
        unsigned int primask;
        asm volatile ("mrs %0, primask" : "=r" (primask));
        if(primask) return 0;
        return 1;
}
#endif

static dyn_SWI* dyn_SWI_LIST[SWI_MAXIMUM_ALLOWED];
static dyn_SWI* dyn_SWI_EXEC[SWI_MAXIMUM_ALLOWED];

/**
 * Execute queued class ISR routines.
 */
void softISR(void) {

        //
        // TO-DO: Perhaps limit to 8, and inline this?
        //


        // Make a working copy, while clearing the queue.
        noInterrupts();
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                dyn_SWI_EXEC[i] = dyn_SWI_LIST[i];
                dyn_SWI_LIST[i] = NULL;
        }
        __DSB();
        interrupts();

        // Execute each class SWI
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                if(dyn_SWI_EXEC[i]) {
#if defined(__DYN_SWI_DEBUG_LED__)
                        digitalWrite(__DYN_SWI_DEBUG_LED__, LOW);
#endif
                        dyn_SWI_EXEC[i]->dyn_SWISR();
                        //dyn_SWI* klass = dyn_SWI_EXEC[i];
                        //klass->dyn_SWISR();
#if defined(__DYN_SWI_DEBUG_LED__)
                        digitalWrite(__DYN_SWI_DEBUG_LED__, HIGH);
#endif
                }
        }
}

/**
 * Initialize the Dynamic (class) Software Interrupt
 */
static void Init_dyn_SWI(void) {
#if defined(__USE_CMSIS_VECTORS__)
        uint32_t *X_Vectors = (uint32_t*)SCB->VTOR;
        for(int i = 0; i < VECTORTABLE_SIZE; i++) {
                _VectorsRam[i] = reinterpret_cast<void (*)()>(X_Vectors[i]); /* copy vector table to RAM */
        }
        /* relocate vector table */
        noInterrupts();
        SCB->VTOR = reinterpret_cast<uint32_t>(&_VectorsRam);
        __DSB();
        interrupts();
#endif
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) dyn_SWI_LIST[i] = NULL;
        noInterrupts();
        _VectorsRam[SWI_IRQ_NUM + 16] = reinterpret_cast<void (*)()>(softISR);
        __DSB();
        interrupts();
        NVIC_SET_PRIORITY(SWI_IRQ_NUM, 255);
        NVIC_ENABLE_IRQ(SWI_IRQ_NUM);
#if defined(__DYN_SWI_DEBUG_LED__)
        pinMode(__DYN_SWI_DEBUG_LED__, OUTPUT);
        digitalWrite(__DYN_SWI_DEBUG_LED__, HIGH);
#endif
}

/**
 *
 * @param klass class that extends dyn_SWI
 * @return 0 on queue full, else returns queue position (ones based)
 */
int exec_SWI(const dyn_SWI* klass) {
        int rc = 0;
        uint8_t irestore = interruptsStatus();
        // Allow use from inside a critical section...
        // ... and prevent races if also used inside an ISR
        noInterrupts();
        for(int i = 0; i < SWI_MAXIMUM_ALLOWED; i++) {
                if(!dyn_SWI_LIST[i]) {
                        rc = 1 + i; // Success!
                        dyn_SWI_LIST[i] = (dyn_SWI*)klass;
                        if(!NVIC_GET_PENDING(SWI_IRQ_NUM)) NVIC_SET_PENDING(SWI_IRQ_NUM);
                        __DSB();
                        break;
                }
        }
        // Restore interrupts, if they were on.
        if(irestore) interrupts();
        return rc;
}

#endif /* defined(__arm__) */
#endif	/* SWI_INLINE_H */
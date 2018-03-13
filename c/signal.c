/* signal.c - support for signal handling
   This file is not used until Assignment 3
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <i386.h>

extern char     *maxaddr;       /* end of memory space */
extern long freemem;
extern pcb* findPCB(int pid);


int sighandler(pcb* process, int signal, void (*newHandler)(void *), void (** oldHandler)(void *)){
    /* check that signal is valid */
    /* signal 31 cannot have handler changed since it is always sysstop */
    if ((signal < 0) || (signal > (SIG_TABLE_SIZE - 1)) || (signal == 31)) {
        return -1;
    }

    /* check that address is in valid memory */
    if (((unsigned long) newHandler >= HOLESTART) && ((unsigned long) newHandler <= HOLEEND)) {
        return -2;
    }

    /* check that handler is in valid memory */
    if (((unsigned long) newHandler >= HOLESTART) && ((unsigned long) newHandler <= HOLEEND)) {
        return -3;
    }

   /* set old handler to point at old handler in signal table*/
   *oldHandler = process->sigHandlerTable[signal];

   /* set handler in signal table to point to new address */
   //kprintf("pid:%d, older handler:%d, newer handler:%d\n", process->pid, process->sigHandlerTable[signal], newHandler);
   process->sigHandlerTable[signal] = newHandler;

   /* add signal to handledSignals */
   process->handledSignals = process->handledSignals | (1u << signal);
   return 0;
}

void sigtramp(void (*handler)(void *), void *cntx) {
    //kprintf("Running the sigtramp code.\n");
    //kprintf("sigtramp handler addr:%d, cntx:%d\n", handler, cntx);
    handler(cntx);
    syssigreturn(cntx);
}

int signal(unsigned int pid, int signalNumber) {
    //kprintf("Sending signal %d to %d\n" , signalNumber, pid);

    /* bad params */
    if (signalNumber < 0 || signalNumber > (SIG_TABLE_SIZE - 1)) {
        return -2;
    }
    if (pid < 0) {
        return -1;
    }

    /* get process */
    pcb* process = findPCB(pid);
    //kprintf("pid at time of signal = %d\n", process->pid);

    /* process does not exist */
    if ((process == NULL) || (pid == 0) || (process->state == STATE_STOPPED)) {
        //kprintf("process has stopped\n");
        return -1;
    }

    /* no handler - return -3 */
    if (process->sigHandlerTable[signalNumber] == NULL) {
        kprintf("no handler\n");
        return -3;
    }

    /* if there is a handler for the signal, register incoming signals */
    if (process->handledSignals & (1u << signalNumber)) {
        process->incomingSignals = process->incomingSignals | (1u << signalNumber);
        //kprintf("incoming signal flag: %d\n", process->incomingSignals);
    }

    /* if process is blocked on a system call, unblock it*/
    if ((process->state != STATE_RUNNING) || (process->state != STATE_READY)) {
        process->ret = -99;
    } else {
        process->ret = 0;
    }

    if ((process->state != STATE_READY) && (process->state != STATE_STOPPED)){
        ready(process);
    }
    return 0;
}

int get_highest(pcb* p) {
    int i = 31;
    while (i >= 0) {
        if (p->incomingSignals & (1u << i)) {
            return i;
        }
        i--;
    }
    return -1;
}

void deliver_signal(pcb* p) {
    context_frame* new_context;
    signal_frame* sig_frame;
    int sigNumber;
    int sigInt;

    // check if there is signal to deliver
    if (p->incomingSignals) {
        //kprintf("incomingSignals when signal comes:%d\n", p->incomingSignals);
        sigNumber = get_highest(p);
        //kprintf("Delivering sigNumber; %d\n", sigNumber);

        /* Put signal_frame onto context */
        new_context = (context_frame*)p->esp - sizeof(signal_frame);
        new_context->iret_eip = sigtramp;
        new_context->iret_cs = getCS();
        new_context->eflags = 0x3200;

//        sig_frame = (signal_frame*) (p->esp - sizeof (signal_frame));
 //       sig_frame->new_cntx
        sig_frame = (signal_frame*) new_context;
        sig_frame->return_address = NULL;
        sig_frame->handler = p->sigHandlerTable[sigNumber];
        sig_frame->cntx = p->esp;
        sig_frame->old_sp = p->esp;
        sig_frame->old_return = p->ret;
        //kprintf("new_context added:%d, iret:%d, cs:%d, sig_frame:%d, handler:%d, cntx:%d\n", new_context, new_context->iret_eip, new_context->iret_cs, sig_frame, sig_frame->handler, sig_frame->cntx);
        new_context->esp = &sig_frame->return_address;
        p->esp = (unsigned int) new_context;

        //Get signal bit
        sigInt = 1 << sigNumber;
        // Clear incomingSignal
        p->incomingSignals = p->incomingSignals & ~sigInt;

        //Disable 15 after it's been sent
        if (sigNumber == 15) {
            p->handledSignals = p->handledSignals & ~(1 << sigNumber);
        }

        //kprintf("New incomingSignals: %d\n", p->incomingSignals);

        //kprintf("done the delivery\n");
    }
}

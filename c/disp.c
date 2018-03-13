/* disp.c : dispatcher
  kprintf("idleproc created\n");
 */

#include <xeroskernel.h>
#include <i386.h>
#include <xeroslib.h>
#include <stdarg.h>
#include <kbd.h>


extern void keyboard_int_read(void);

static pcb      *head = NULL;
static pcb      *tail = NULL;

extern pcb *findPCB( int pid ) {
/******************************/

    int i;

    for( i = 0; i < MAX_PROC; i++ ) {
        if( proctab[i].pid == pid ) {
            return( &proctab[i] );
        }
    }

    return( NULL );
}

//add p to procToExit's waiting Q
void addToWaiting(pcb* procToExit, pcb* proc) {
    if( procToExit->waitingTail) {
        procToExit->waitingTail->next = proc;
    } else {
        procToExit->waiting = proc;
    }
    procToExit->waitingTail = proc;
    proc->next = NULL;
    //kprintf("process %d is waiting for process %d to exit\n", proc->pid, procToExit->pid);
}

// set all the processes sigwaiting on proc to ready
void removeWaiting(pcb* proc) {
    pcb* temp;
    while (proc->waiting) {
        temp = proc->waiting;
        proc->waiting = proc->waiting->next;
        temp->next = NULL;
        if (temp->state == STATE_WAITING) {
            //kprintf("setting waiting pid %d back onto readyq\n", proc->waiting->pid);
            ready(temp);
        }
        if (proc->waiting == proc->waitingTail) {
            break;
        }
    }
    proc->waiting = NULL;
    proc->waitingTail = NULL;
}


void     dispatch( void ) {
/********************************/

    pcb         *p;
    int         r;
    funcptr     fp;
    int         stack;
    va_list     ap;
    char        *str;
    int         len;
    int         pid;
    int         fd;
    int         bufflen;
    void*       buff;
    int         result;

    for( p = next(); p; ) {
      deliver_signal(p);
      p->state = STATE_RUNNING;
      r = contextswitch( p );
      switch( r ) {
      case( SYS_CREATE ):
        ap = (va_list)p->args;
        fp = (funcptr)(va_arg( ap, int ) );
        stack = va_arg( ap, int );
            p->ret = create( fp, stack );
        break;
      case( SYS_YIELD ):
        ready( p );
        p = next();
        break;
      case( SYS_STOP ):
        p->state = STATE_STOPPED;
        //ready all processes waiting on this process to die
        removeWaiting(p);
        //kprintf("PROCESS %d SHOULD BE STOPPED NOW\n", p->pid);
        //kprintf("next on readyq: %d\n", head->pid);
        p = next();
        break;
      case (SYS_KILL):
        ap = (va_list)p->args;
        pid = va_arg(ap, int);
        int sigNum = va_arg(ap, int);

        //kprintf("SYSKILL SIGNAL PID: %d, SIGNUM:%d\n", pid, sigNum);
        result = signal(pid, sigNum);

        /* target process does not exist, return -512 */
        if (result == -1) {
            p->ret = -512;
        } else if (result == -2) {
            /* signal number invalid */
            p->ret = -561;
        } else {
            p->ret = 0;
        }
            break;
      case (SYS_CPUTIMES):
        ap = (va_list) p->args;
        p->ret = getCPUtimes(p, va_arg(ap, processStatuses *));
        break;
      case( SYS_PUTS ):
        ap = (va_list)p->args;
        str = va_arg( ap, char * );
        kprintf( "%s", str );
        p->ret = 0;
        break;
      case( SYS_GETPID ):
        p->ret = p->pid;
        break;
      case( SYS_SLEEP ):
        ap = (va_list)p->args;
        len = va_arg( ap, int );
        sleep( p, len );
        p = next();
        break;
      case( SYS_TIMER ):
        tick();
        //kprintf("T");
        p->cpuTime++;
        ready( p );
        p = next();
        end_of_intr();
        break;
      case (SYS_SIGHANDLER):
        ap = (va_list) p->args;
        int signal = va_arg(ap, int);
        void* newHandler = va_arg(ap, void*);
        void* oldHandler = va_arg(ap, void*);
        p->ret = sighandler(p, signal, newHandler, oldHandler);
        break;
      case (SYS_SIGRETURN):
        //kprintf("SYS_SIGRETURN called\n");
        ap = (va_list) p->args;
        /* restore old pointers */
        unsigned int old_sp = va_arg(ap, void*);
        p->esp = (unsigned int*)old_sp;
        //kprintf("sigreturn esp: %d\n", p->esp);
        signal_frame* sig_frame = (signal_frame*)(p->esp - sizeof(signal_frame));
        //kprintf("sigreturn sig_frame:%d\n", sig_frame);
        p->ret = sig_frame->old_return;
        //kprintf("sigreturn pret:%d\n", p->ret);
        break;
      case (SYS_WAIT):
        ap = (va_list) p->args;
        pid = va_arg(ap, int);
        pcb* targetPCB;

        /* process 0 considered not to exist */
        if (pid == 0) {
            p->ret =  -1;
            break;
        }
        /* process does not exist */
        if (!(targetPCB == findPCB( pid ))) {
            p->ret =  -1;
            break;
        }
        if (targetPCB->state == STATE_STOPPED) {
            //kprintf("waiting on process that is stopped. exit \n");
            p->ret =  -1;
            break;
        }
        /* otherwise */
        addToWaiting(targetPCB, p);
        p->state = STATE_WAITING;
        p = next();
        break;
      case (SYS_OPEN):
        ap = (va_list) p->args;
        int deviceNum = va_arg(ap, int);
        p->ret = di_open(p, deviceNum);
        break;
      case (SYS_CLOSE):
        ap = (va_list) p->args;
        fd = va_arg(ap, int);
        p->ret = di_close(p, fd);
        break;
      case (SYS_READ):
        ap = (va_list) p->args;
        fd = va_arg(ap, int);
        buff = va_arg(ap, void*);
        bufflen = va_arg(ap, int);
        result = di_read(p, fd, buff, bufflen);
        //kprintf("RESULT OF SYS_READ:%d\n", result);
        if (result == DEV_BLOCK) {
            p->state = STATE_BLOCKED_READING;
            p = next();
        } else if (result == DEV_ERROR){
            p->ret = DEV_ERROR;
        }
        break;
      case (SYS_WRITE):
        ap = (va_list) p->args;
        fd = va_arg(ap, int);
        buff = va_arg(ap, void*);
        bufflen = va_arg(ap, int);
        result = di_write(p, fd, buff, bufflen);
        if (result == DEV_BLOCK) {
            p->state = STATE_BLOCKED_WRITING;
            p = next();
        } else {
            p->ret = result;
        }
        break;
      case (SYS_IOCTL):
        ap = (va_list) p->args;
        fd = va_arg(ap, int);
        unsigned long command = va_arg(ap, unsigned long);
        va_list args = va_arg(ap, va_list);
        p->ret = di_ioctl(p, fd, command, args);
        break;
      case (SYS_KEYBOARD):
        //kprintf("SYS_KEYBOARD WAS CALLED\n");
        result = kbd_int_read();
        if (result == DEV_BLOCK) {
            p->state = STATE_BLOCKED_READING;
        } else if (result == DEV_ERROR){
            p->ret = DEV_ERROR;
        }
        end_of_intr();
        break;
      default:
        kprintf( "Bad Sys request %d, pid = %d\n", r, p->pid );
      }
    }

    kprintf( "Out of processes: dying\n" );

    for( ;; );
}

extern void dispatchinit( void ) {
/********************************/

  //bzero( proctab, sizeof( pcb ) * MAX_PROC );
  memset(proctab, 0, sizeof( pcb ) * MAX_PROC);
}



extern void     ready( pcb *p ) {
/*******************************/

    p->next = NULL;
    p->state = STATE_READY;

    if( tail ) {
        tail->next = p;
    } else {
        head = p;
    }

    tail = p;
}

extern pcb      *next( void ) {
/*****************************/

    pcb *p;

    p = head;

    if( p ) {
        head = p->next;
        if( !head ) {
            tail = NULL;
        }
    } else { // Nothing on the ready Q and there should at least be the idle proc.
        //kprintf( "BAD: NOTHING ON READY Q\n" );
        for(;;);
    }

    p->next = NULL;
    p->prev = NULL;
    return( p );
}

// This function takes a pointer to the pcbtab entry of the currently active process.
// The functions purpose is to remove the process being pointed to from the ready Q
// A similar function exists for the management of the sleep Q. Things should be re-factored to
// eliminate the duplication of code if possible. There are some challenges to that because
// the sleepQ is a delta list and something more than just removing an element in a list
// is being preformed.


void removeFromReady(pcb * p) {

  if (!head) {
    kprintf("Ready queue corrupt, empty when it shouldn't be\n");
    return;
  }

  if (head == p) { // At front of list
    // kprintf("Pid %d is at front of list\n", p->pid);
    head = p->next;

    // If the implementation has idle on the ready list this next statement
    // isn't needed. However, it is left just in case someone decides to
    // change things so that the idle process is kept separate.

    if (head == NULL) { // If the implementation has idle process  on the
      tail = head;      // ready list this should never happen
      kprintf("Kernel bug: Where is the idle process\n");
    }
  } else {  // Not at front, find the process.
    pcb * prev = head;
    pcb * curr;

    for (curr = head->next; curr!=NULL; curr = curr->next) {
      if (curr == p) { // Found process so remove it
        // kprintf("Found %d in list, removing\n", curr->pid);
        prev->next = p->next;
        if (tail == p) { //last element in list
            tail = prev;
            // kprintf("Last element\n");
        }
        p->next = NULL; // just to clean things up
        break;
      }
      prev = curr;
    }
    if (curr == NULL) {
      kprintf("Kernel bug: Ready queue corrupt, process %d claimed on queue and not found\n",
              p->pid);

    }
  }
}

extern char * maxaddr;

int getCPUtimes(pcb *p, processStatuses *ps) {

  int i, currentSlot;
  currentSlot = -1;

  // Check if address is in the hole
  if (((unsigned long) ps) >= HOLESTART && ((unsigned long) ps <= HOLEEND))
    return -1;

  //Check if address of the data structure is beyone the end of main memory
  if ((((char * ) ps) + sizeof(processStatuses)) > maxaddr)
    return -2;

  // There are probably other address checks that can be done, but this is OK for now

  for (i=0; i < MAX_PROC; i++) {
    if (proctab[i].state != STATE_STOPPED) {
      // fill in the table entry
      currentSlot++;
      ps->pid[currentSlot] = proctab[i].pid;
      ps->status[currentSlot] = p->pid == proctab[i].pid ? STATE_RUNNING: proctab[i].state;
      ps->cpuTime[currentSlot] = proctab[i].cpuTime * MILLISECONDS_TICK;
    }
  }

  return currentSlot;
}

// This function takes 2 paramenters:
//  currP  - a pointer into the pcbtab that identifies the currently running process
//  pid    - the proces ID of the process to be killed.
//
// Note: this function needs to be augmented so that it delivers a kill signal to a
//       a particular process. The main functionality of the this routine will remain the
//       same except that when the process is located it needs to be put onto the readyq
//       and a signal needs to be marked for delivery.
//

/*
static int  kill(pcb* p, int pid, int signalNumber) {
  pcb * targetPCB;

  if (!(targetPCB = findPCB( pid ))) {
    // kprintf("Target pid not found\n");
    return -512;
  }

  if (targetPCB->state == STATE_STOPPED) {
    kprintf("Target pid was stopped\n");
    return -512;
  }

  // PCB has been found,  and the proces is either sleepign or running.
  // based on that information remove the process from
  // the appropriate queue/list.

  if (targetPCB->state == STATE_SLEEP) {
    // kprintf("Target pid %d sleeping\n", targetPCB->pid);
    removeFromSleep(targetPCB);
  }

  if (targetPCB->state == STATE_READY) {
    // remove from ready queue
    // kprintf("Target pid %d is ready\n", targetPCB->pid);
    removeFromReady(targetPCB);
  }

  // Check other states and do state specific cleanup before stopping
  // the process
  // In the new version the process will not be marked as stopped but be
  // put onto the readyq and a signal marked for delivery.

  targetPCB->state = STATE_STOPPED;
  return 0;
}
*/

// This function is the system side of the sysgetcputimes call.
// It places into a the structure being pointed to information about
// each currently active process.
//  p - a pointer into the pcbtab of the currently active process
//  ps  - a pointer to a processStatuses structure that is
//        filled with information about all the processes currently in the system
//


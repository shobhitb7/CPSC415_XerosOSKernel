/* xeroskernel.h - disable, enable, halt, restore, isodd, min, max */

#ifndef XEROSKERNEL_H
#define XEROSKERNEL_H

#include <stdarg.h>

/* Symbolic constants used throughout Xinu */
typedef char    Bool;        /* Boolean type                  */
typedef unsigned int size_t; /* Something that can hold the value of
                              * theoretical maximum number of bytes
                              * addressable in this architecture.
                              */
#define FALSE   0       /* Boolean constants             */
#define TRUE    1
#define EMPTY   (-1)    /* an illegal gpq                */
#define NULL    0       /* Null pointer for linked lists */
#define NULLCH '\0'     /* The null character            */

#define CREATE_FAILURE -1  /* Process creation failed     */

/* Universal return constants */

#define OK            1         /* system call ok               */
#define SYSERR       -1         /* system call failed           */
#define EOF          -2         /* End-of-file (usu. from read) */
#define TIMEOUT      -3         /* time out  (usu. recvtim)     */
#define INTRMSG      -4         /* keyboard "intr" key pressed  */
                                /*  (usu. defined as ^B)        */
#define BLOCKERR     -5         /* non-blocking op would block  */

/* Functions defined by startup code */


void           bzero(void *base, int cnt);
void           bcopy(const void *src, void *dest, unsigned int n);
void           disable(void);
unsigned short getCS(void);
unsigned char  inb(unsigned int);
void           init8259(void);
int            kprintf(char * fmt, ...);
void           lidt(void);
void           outb(unsigned int, unsigned char);


/* Some constants involved with process creation and managment */

#define STARTING_EFLAGS         0x00003000
#define ARM_INTERRUPTS          0x00000200

   /* Maximum number of processes */
#define MAX_PROC        64
   /* Kernel trap number          */
#define KERNEL_INT      80
   /* Interrupt number for the timer */
#define TIMER_INT      (TIMER_IRQ + 32)
   /* Minimum size of a stack when a process is created */
#define KEYBOARD_INT    (1 + 32)
   /* Minimum size of a stack when a process is created */
#define PROC_STACK      (4096 * 4)
    /* Max Number of devices */
#define NUM_FD          4
    /* Number of devices */
#define NUM_DEVICES     2
    /* Keyboard devices */
#define KB_WITHOUT_ECHO 0
#define KB_WITH_ECHO 1


   /* Number of milliseconds in a tick */
#define MILLISECONDS_TICK 10

    /* Table size of signal table in each process */
#define SIG_TABLE_SIZE 32

/* Constants to track states that a process is in */
#define STATE_STOPPED   0
#define STATE_READY     1
#define STATE_SLEEP     22
#define STATE_RUNNING   23
#define STATE_WAITING   24
#define STATE_BLOCKED_READING   25
#define STATE_BLOCKED_WRITING   26

/* System call identifiers */
#define SYS_STOP        10
#define SYS_YIELD       11
#define SYS_CREATE      22
#define SYS_TIMER       33
#define SYS_GETPID      144
#define SYS_PUTS        155
#define SYS_SLEEP       166
#define SYS_KILL        177
#define SYS_CPUTIMES    178
#define SYS_SIGHANDLER  188
#define SYS_SIGRETURN   189
#define SYS_WAIT        190
#define SYS_OPEN        200
#define SYS_CLOSE       201
#define SYS_WRITE       202
#define SYS_READ        203
#define SYS_IOCTL       204
#define SYS_KEYBOARD    205

/* Device call states */
#define DEV_BLOCK       1
#define DEV_DONE        0
#define DEV_ERROR       -1

/* Structure to track the information associated with a single process */

typedef struct struct_pcb pcb;
typedef struct devsw {
    int (*dvopen)(pcb*, int);
    int (*dvclose)(pcb*, int);
    int (*dvread)(pcb*, void*, int);
    int (*dvwrite)(pcb*, void*, int);
    int (*dvioctl)(pcb*, unsigned long, ...);
} devsw;

devsw deviceTable[NUM_FD];

struct struct_pcb {
  void        *esp;    /* Pointer to top of saved stack           */
  pcb         *next;   /* Next process in the list, if applicable */
  pcb         *prev;   /* Previous proccess in list, if applicable*/
  int          state;  /* State the process is in, see above      */
  unsigned int pid;    /* The process's ID                        */
  int          ret;    /* Return value of system call             */
                       /* if process interrupted because of system*/
                       /* call                                    */
  long         args;
  unsigned int otherpid;
  void        *buffer;
  int          bufferlen;
  int          sleepdiff;
  long         cpuTime;  /* CPU time consumed                     */
  unsigned int sigHandlerTable[SIG_TABLE_SIZE];
  unsigned int incomingSignals; //signals being received
  unsigned int handledSignals; //signals that have handlers
  int openedDvTable[NUM_FD]; //file descriptor table
  pcb* waiting; //processes waiting for this process to stop
  pcb* waitingTail;

};

typedef struct struct_ps processStatuses;
struct struct_ps {
  int  entries;            // Last entry used in the table
  int  pid[MAX_PROC];      // The process ID
  int  status[MAX_PROC];   // The process status
  long  cpuTime[MAX_PROC]; // CPU time used in milliseconds
};


/* The actual space is set aside in create.c */
extern pcb     proctab[MAX_PROC];

#pragma pack(1)

/* What the set of pushed registers looks like on the stack */
typedef struct context_frame {
  unsigned long        edi;
  unsigned long        esi;
  unsigned long        ebp;
  unsigned long        esp;
  unsigned long        ebx;
  unsigned long        edx;
  unsigned long        ecx;
  unsigned long        eax;
  unsigned long        iret_eip;
  unsigned long        iret_cs;
  unsigned long        eflags;
  unsigned long        stackSlots[];//stackSlots[0] = return
} context_frame;

typedef struct signal_frame {
    context_frame new_cntx;
    unsigned int return_address;
    unsigned int handler;
    unsigned int cntx;
    unsigned int old_sp;
    unsigned int old_sigPriority;
    unsigned int old_return;
} signal_frame;

/* Memory mangement system functions, it is OK for user level   */
/* processes to call these.                                     */

int      kfree(void *ptr);
void     kmeminit( void );
void     *kmalloc( size_t );


/* A typedef for the signature of the function passed to syscreate */
typedef void    (*funcptr)(void);


/* Internal functions for the kernel, applications must never  */
/* call these.                                                 */
void     dispatch( void );
void     dispatchinit( void );
void     ready( pcb *p );
pcb      *next( void );
void     contextinit( void );
int      contextswitch( pcb *p );
pcb*     findPCB(int pid);
int      create( funcptr fp, size_t stack );
void     printCF (void * stack);  /* print the call frame */
int      syscall(int call, ...);  /* Used in the system call stub */
void     sleep(pcb *, unsigned int);
void     removeFromSleep(pcb * p);
void     tick( void );
int      getCPUtimes(pcb *p, processStatuses *ps);
int      sighandler(pcb *p, int signal, void (*newhandler)(void *), void (** oldHandler)(void *));
void     sigtramp(void (*handler)(void *), void *cntx);
int      signal(unsigned int pid, int signalNumber);
void     devinit(void);


/* Function prototypes for system calls as called by the application */
int          syscreate( funcptr fp, size_t stack );
void         sysyield( void );
void         sysstop( void );
unsigned int sysgetpid( void );
unsigned int syssleep(unsigned int);
void         sysputs(char *str);
int          sysgetcputimes(processStatuses *ps);
int          syssighandler(int signal, void (*newhandler)(void *), void (** oldHandler)(void *));
void         syssigreturn(void *old_sp);
int          syskill(int PID, int signalNumber);
int          syswait(int PID);
void         deliver_signal(pcb* p);
int          sysopen(int deviceNumber);
int          sysclose(int fd);
int          syswrite(int fd, void *buff, int buffLen);
int          sysread(int fd, void *buff, int bufflen);
int          sysioctl(int fd, unsigned long command, ...);

/* device calls */
extern int di_open(pcb* proc, int device_no);
extern int di_close(pcb* proc, int fd);
extern int di_write(pcb* proc, int fd, void* buff, int bufflen);
extern int di_read(pcb* proc, int fd, void* buff, int bufflen);
extern int di_ioctl(pcb* proc, int fd, unsigned long command, va_list args);

/* The initial process that the system creates and schedules */
void    root( void );
void    shell(void);
void    t_func(void);
void    a_func(void);
void    set_evec(unsigned int xnum, unsigned long handler);

/* test functions */
void root_testing_sysopen(void);
void root_testing_sysclose(void);
void root_testing_syswrite(void);
void root_testing_sysioctl(void);
void root_testing_signal(void);
void root_testing_syssighandler(void);
void root_testing_syskill(void);
void root_testing_prioritysigs(void);
void interrupted_by_sig(void);
void root_test(void);
void sig(void);
void test_sig(void);
void blocked_wait_sig(void);
void send_sig_lo(void);
void send_sig_mid(void);
void send_sig_hi(void);
void test(void);
void handler_exit(void* ctx);
void handler_nothing(void *cntx);
void busy(void);
void idle_wait_sig(void);
void loop_wait_sig(void);
void stack_sigtramp(void);


/* Anything you add must be between the #define and this comment */
#endif

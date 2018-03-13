#include <xeroskernel.h>
#include <kbd.h>
#include <i386.h>
#include <icu.h>
#include <stdarg.h>

#define ENTERKEY 0xA

static unsigned int kbtoa( unsigned char code );
extern void enable_irq(unsigned int, int);
extern long     freemem;

///*  Normal table to translate scan code  */
unsigned char   kbcode[] = { 0,
          27,  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',
         '0',  '-',  '=', '\b', '\t',  'q',  'w',  'e',  'r',  't',
         'y',  'u',  'i',  'o',  'p',  '[',  ']', '\n',    0,  'a',
         's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';', '\'',
         '`',    0, '\\',  'z',  'x',  'c',  'v',  'b',  'n',  'm',
         ',',  '.',  '/',    0,    0,    0,  ' ' };

/* captialized ascii code table to tranlate scan code */
unsigned char   kbshift[] = { 0,
           0,  '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',
         ')',  '_',  '+', '\b', '\t',  'Q',  'W',  'E',  'R',  'T',
         'Y',  'U',  'I',  'O',  'P',  '{',  '}', '\n',    0,  'A',
         'S',  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',
         '~',    0,  '|',  'Z',  'X',  'C',  'V',  'B',  'N',  'M',
         '<',  '>',  '?',    0,    0,    0,  ' ' };

/* extended ascii code table to translate scan code */
unsigned char   kbctl[] = { 0,
           0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
           0,   31,    0, '\b', '\t',   17,   23,    5,   18,   20,
          25,   21,    9,   15,   16,   27,   29, '\n',    0,    1,
          19,    4,    6,    7,    8,   10,   11,   12,    0,    0,
           0,    0,   28,   26,   24,    3,   22,    2,   14,   13 };

void kbd_reset_state(void) {
    // reset state of keyboard
    open = 0;
    // reset type of keyboard
    echo = 0;
    // reset eof flag
    eof_flag = 0;
    // reset eof
    eof = EOF; //CTRL-D
    // reset process with keyboard
    kbd_proc = NULL;
    // reset number of chars read
    p_read = 0;
    // reset process buf
    p_buf = NULL;
    // reset process buf len
    p_len = 0;
    //reset kbd buffer
    for (int i = 0; i < 4; i++) {
        kbd_buf[i] = 0;
    }
    // reset kbd length
    kbd_len = 0;
    // reset kbd index
    kbd_index = 0;
}

int kbd_open(pcb* process, int echo_flag) {
    //check if  a keyboard is open already
    if (open){
        kprintf("A device is already open\n");
        return DEV_ERROR;
    }

    // set state - device[0] will not echo
    kbd_reset_state();
    kbd_proc = process;
    open = 1;
    echo = echo_flag;

    // enable keyboard
    outb(0xAE, 0x64);
    // enable interrupts
    enable_irq(1,0);

    return 0;
}

int kbd_close(pcb* process, int dev_no) {
    //check if  a keyboard is open
    if (!open){
        return DEV_ERROR;
    }
    // disable keyboard
    outb(0xAD, 0x64);
    // disable interrupts
    enable_irq(1,1);
    //reset state
    kbd_reset_state();

    return DEV_DONE;
}

int transfer(void) {
    char c;
    // don't read more than process asks for
    // don't read more than what is in the buffer
    while ((kbd_len > 0) && (p_read < p_len)){

        c = kbd_buf[kbd_index];

        // decrease the amount of char in kbd_buf
        kbd_len--;
        //increase index for next time
        kbd_index = (kbd_index + 1) % KBD_BUF_SIZE;

        // check to see if char is eof
        if (c == eof) {
            eof_flag = 1;
            kbd_proc->ret = p_read;
            //kprintf("got eof. set proc return to read:%d\n", p_read);
            ready(kbd_proc);
            return DEV_DONE;
        }
        //if echo, print out char
        if (echo) {
            kprintf("%c", c);
        }
        // add char to process buffer
        if (c == '\n') {
            //kprintf("ADDING NULL TERMINATED STRING TO P_BUF\n");
            p_buf[p_read++] = '\0';
            //kprintf("BUFSOFAR:%s, LEN :%d\n", p_buf, p_read);
            kbd_proc->ret = p_read;
            ready(kbd_proc);
            //kprintf("set proc return to read:%d\n", p_read);
            return DEV_DONE;
        } else {
            p_buf[p_read++] = c;
        }
    }

    // if amount left to read is greater than 0, block
    //kprintf("IN TRANSFER p_len:%d, p_read:%d\n", p_len, p_read);
    if (p_len - p_read) {
        //update amount read in case of signal
        kbd_proc->ret = p_read;
        /*
        if (p_read == 0) {
            kprintf("setting return value to -99 in transfer\n");
            kbd_proc->ret = -99;
        }
        */
        return DEV_BLOCK;
    } else {
        // otherwise happy
        eof_flag = 1;
        kbd_proc->ret = p_read;
        //kprintf("set proc return to read:%d\n", p_read);
        ready(kbd_proc);
        return DEV_DONE;
    }
}

int kbd_read(pcb* process, void* buf, int buflen){
    //kprintf("reading process pid:%d, buf:%s, buflen:%d\n", process->pid, buf, buflen);
    // verify that keyboard is open and correct process
    if (!open){
        //kprintf("open: %d, proc: %d, kbd_proc:%d\n", open, process->pid, kbd_proc->pid);
        //kprintf("DEVICE NOT OPEN/NO PROC\n");
        return DEV_ERROR;
    }
    // verify that address is in a valid space
    if (((unsigned long) buf) >= HOLESTART && ((unsigned long) buf <= HOLEEND)) {
        //kprintf("BAD ADDRESS FOR BUF\n");
        return DEV_ERROR;
    }
    //verify that buflen >= 0
    if (buflen < 0) {
        //kprintf("Buflen <0\n");
        return DEV_ERROR;
    }

    //check if eof was reached already
    if (eof_flag) {
        //kprintf("eof already reached");
        // reset read because nothing was read
        p_read = 0;
        kbd_proc->ret = p_read;
        return DEV_DONE;
    }

    // set process buff and process len
    // reset the read amount and eof_flag in case of 2 subsequent reads
    p_buf = buf;
    p_len = buflen;
    p_read = 0;
    eof_flag = 0;

    // if kernel buffer has data available, get as much possible
    if ((kbd_buf[kbd_index] != NULL) && (kbd_len > 0) && p_read != p_len) {
        //kprintf("transfering data");
        int result = transfer();
        return result;
    }
    //otherwise block
    kbd_proc->ret = p_read;
    /*
    if (p_read == 0) {
        kprintf("setting return to -99 because nothing read\n");
        kbd_proc->ret = -99;
    }
    */
    //kprintf("KBD READ RETURNING DEV_BLOCK\n");
    return DEV_BLOCK;
}

// write is not supported
int kbd_write(pcb* process, void* buf, int buflen){
    return DEV_ERROR;
}

int kbd_ioctl(pcb* process, unsigned long command, va_list args) {
    if (command == 53) {
        /* set new eof indicator */
        eof = va_arg(args, int);
        //kprintf("eof:%d\n", eof);//delete
        return DEV_DONE;
    } else if (command == 55) {
        /* set echo off */
        //kprintf("setting echo to 0\n"); //delete
        echo = 0;
        return DEV_DONE;
    } else if (command == 56) {
        /* set echo on */
        //kprintf("setting echo to 1\n");//delete
        echo = 1;
        return DEV_DONE;
    } else {
        /* no such command */
        return DEV_ERROR;
    }
}

int kbd_int_read() {
    //kprintf("AN INTERRUPT OCCURED\n");
    unsigned char a;

    // check if eof_flag is on
    if (eof_flag) {
        //kprintf("eof flag is on\n");
        p_read = 0;
        kbd_proc->ret = p_read;
        ready(kbd_proc);
        return DEV_DONE;
    }

    //check if data is ready and read port to kbd_buffer
    unsigned char byte = inb(0x64);
    if (byte & 1) {
        // get scan code from port
        byte = inb(0x60);
        //kprintf("PORT BYTE READ:%d\n", byte);
        // get ascii from scan code
        a = kbtoa(byte);

        //get rid of KEY-UP
        if (a && (a != NOCHAR) && kbd_len < 4) {
            //kprintf("This is the char from kbd:%c, %d\n", a, a); //delete

            //special ENTERKEY case
            if (a == ENTERKEY) {
                //kprintf("kbd_index:%d, kbd_len:%d\n", kbd_index, kbd_len); //delete
                kbd_buf[(kbd_index + kbd_len) % KBD_BUF_SIZE] = '\n';
                kbd_len++;
            }
            //otherwise
            else if (kbd_len <= 3) {
                //kprintf("kbd_index:%d, kbd_len:%d\n", kbd_index, kbd_len); //delete
                kbd_buf[(kbd_index + kbd_len) % KBD_BUF_SIZE] = a;
                kbd_len++;
            }

            //if there is a process blocked, do a transfer
            //if a process is blocked, assume address of p_buf is valid since read already check it earlier
            if ((kbd_proc != NULL) && (kbd_proc->state == STATE_BLOCKED_READING) && p_buf != NULL) {
                //kprintf("GOT AN INTERRUPT SO ADDING IT TO THE APPLICATION\n");
                int result = transfer();
                return result;
            }
        }
    }
    // if no data is ready, keep blocking
    kbd_proc->ret = p_read;
    /*
    if (p_read == 0) {
        kprintf("no data is ready, setting return to -99\n");
        kbd_proc->ret = -99;
    }
    */
    return DEV_BLOCK;
}

/* FUNCTIONS FROM scanCodesToAscii.txt */

static  int state; /* the state of the keyboard */

static unsigned int extchar( unsigned char code )
{
  return state &= ~EXTENDED;
}

unsigned int kbtoa( unsigned char code )
{
  unsigned int    ch;

  if (state & EXTENDED) {
    return extchar(code);
  }
  if (code & KEY_UP) {
    switch (code & 0x7f) {
      case LSHIFT:
      case RSHIFT:
        state &= ~INSHIFT;
        break;
      case CAPSL:
         state &= ~CAPSLOCK;
        //state = (state & CAPSLOCK) ? (state & ~CAPSLOCK) : (state | CAPSLOCK);
        // commented out
        break;
      case LCTL:
        state &= ~INCTL;
        break;
      case LMETA:
        state &= ~INMETA;
        break;
    }

    return NOCHAR;
  }


  /* check for special keys */
  switch (code) {
    case LSHIFT:
    case RSHIFT:
      state |= INSHIFT;
      return NOCHAR;
    case CAPSL:
      state |= CAPSLOCK; //this was commeted out
      return NOCHAR;
    case LCTL:
      state |= INCTL;
      return NOCHAR;
    case LMETA:
      state |= INMETA;
      return NOCHAR;
    case EXTESC:
      state |= EXTENDED;
      return NOCHAR;
  }

  ch = NOCHAR;

  if (code < sizeof(kbcode)){
    if ( state & CAPSLOCK )
      ch = kbshift[code];
    else
      ch = kbcode[code];
  }
  if (state & INSHIFT) {
    if (code >= sizeof(kbshift))
      return NOCHAR;
    if ( state & CAPSLOCK )
      ch = kbcode[code];
    else
      ch = kbshift[code];
  }
  if (state & INCTL) {
    if (code >= sizeof(kbctl))
      return NOCHAR;
    ch = kbctl[code];
  }
  if (state & INMETA)
    ch += 0x80;
  return ch;
}

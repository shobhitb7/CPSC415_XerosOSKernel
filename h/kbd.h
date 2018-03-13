/* kbd.h - everything keyboard related */
#ifndef KBD_H
#define KBD_H

#include <xeroskernel.h>

#define KBD_BUF_SIZE 4

#define DEFAULT_EOF 4
#define EOF_REACHED 1
#define EOF_IN_BUF  2

#define KEY_UP   0x80            /* If this bit is on then it is a key   */
                                 /* up event instead of a key down event */

/* Control code */
#define LSHIFT  0x2a
#define RSHIFT  0x36
#define LMETA   0x38

#define LCTL    0x1d
#define CAPSL   0x3a

/* scan state flags */
#define INCTL           0x01    /* control key is down          */
#define INSHIFT         0x02    /* shift key is down            */
#define CAPSLOCK        0x04    /* caps lock mode               */
#define INMETA          0x08    /* meta (alt) key is down       */
#define EXTENDED        0x10    /* in extended character mode   */

#define EXTESC          0xe0    /* extended character escape    */
#define NOCHAR  256

/* Global keyboard state */
int open; //if a device is open or closed
int echo; // if device is echo or not
int eof_flag;  // if device reached eof
unsigned int eof; //eof char
pcb* kbd_proc;  //process
char kbd_buf[KBD_BUF_SIZE];  //kernel buffer
int kbd_len; //records the amount of char that have been in buffer
int kbd_index; //index of circular kernel buffer (head of buf)
unsigned char* p_buf; //process buffer
int p_read; //amount of data read from kernel buffer
int p_len; //size of process buffer/amount to read

/* keyboard calls */
int kbd_open(pcb* process, int dev_no);
int kbd_close(pcb* process, int dev_no);
int kbd_read(pcb* process, void* buf, int buflen);
int kbd_write(pcb* process, void* buf, int buflen);
int kbd_ioctl(pcb* process, unsigned long command, va_list args);
int kbd_int_read(void);

/* helper calls */
void kbd_reset_state(void);
int transfer(void);


#endif

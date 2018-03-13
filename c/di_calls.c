/* di_calls.c : Dispatch Calls
 */

#include <xeroskernel.h>
#include <stdarg.h>
#include <kbd.h>

void devinit(void) {

    // device no
    //kbd_device_no = -1;
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
    // reset keyboard buffer
    for (int i= 0; i < KBD_BUF_SIZE; i++) {
        kbd_buf[i] = 0;
    }
    //reset keyboard buffer length
    kbd_len = 0;
    // reset index to buffer transfer
    kbd_index = 0;
    // reset number of chars read
    p_read = 0;
    // reset process buf
    p_buf = NULL;
    // reset process buf len
    p_len = 0;

    /* keyboard without echo (0)*/
    deviceTable[KB_WITHOUT_ECHO].dvopen =  kbd_open;
    deviceTable[KB_WITHOUT_ECHO].dvclose = kbd_close;
    deviceTable[KB_WITHOUT_ECHO].dvread =  kbd_read;
    deviceTable[KB_WITHOUT_ECHO].dvwrite = kbd_write;
    deviceTable[KB_WITHOUT_ECHO].dvioctl=  kbd_ioctl;

    /* keyboard with echo (1)*/
    deviceTable[KB_WITH_ECHO].dvopen =  kbd_open;
    deviceTable[KB_WITH_ECHO].dvclose = kbd_close;
    deviceTable[KB_WITH_ECHO].dvread =  kbd_read;
    deviceTable[KB_WITH_ECHO].dvwrite = kbd_write;
    deviceTable[KB_WITH_ECHO].dvioctl = kbd_ioctl;

}

int di_open(pcb* process, int device_no){
    /* check valid device_no */
    if ((device_no < 0) || (device_no > (NUM_FD - 1))) {
        //kprintf("device no not valid: %d\n", device_no);
        return DEV_ERROR;
    }

    /* otherwise look for empty slot in process deviceTable */
    int fd;
    for (fd = 0; fd < NUM_FD; fd++){
        if (process->openedDvTable[fd] == -1) {
            break;
        }
    }

    /* map the process device to global device table */
    process->openedDvTable[fd] = device_no;

    /* return*/
    deviceTable[device_no].dvopen(process, device_no);
    //kprintf("di_open returned fd of %d\n", fd);
    return fd;
}


int di_close(pcb* process, int fd) {
    /* check valid fd number and that device is open*/
    if (fd < 0 || fd > (NUM_FD - 1) || process->openedDvTable[fd] == -1) {
        //kprintf("problem closing device \n");
        return DEV_ERROR;
    }

    /* get the global device table # */
    int global_dev_no = process->openedDvTable[fd];

    if (deviceTable[global_dev_no].dvclose(process, fd) == 0) {
        process->openedDvTable[fd] = -1;
        return DEV_DONE;
    }

    return DEV_ERROR;
}

int di_read(pcb* process, int fd, void* buff, int bufflen){
    /* check that fd is valid and that device is open*/
    if (fd < 0 || fd >= NUM_FD || (process->openedDvTable[fd] == -1)) {
        //kprintf("fd:%d\n", fd);
        //kprintf("Invalid args\n");
        return DEV_ERROR;
    }

    /* Perform read operation */
    /* Return DEV_BLOCK (1), DEV_DONE (0), DEV_ERROR (-1) */
    //kprintf("DI_READ CALLING KBD_READ\n");
    int global_dev_no = process->openedDvTable[fd];
    //kprintf("glob dev no:%d\n", global_dev_no);
    return deviceTable[global_dev_no].dvread(process, buff, bufflen);
}

int di_write(pcb* process, int fd, void* buff, int bufflen){
    /* check that fd is valid and device is open */
    if (fd < 0 || fd >= NUM_FD || (process->openedDvTable[fd] == -1)) {
        return DEV_ERROR;
    }
    /* perform write operation */
    /* Return DEV_BLOCK (1), DEV_DONE (0), DEV_ERROR (-1) */
    int global_dev_no = process->openedDvTable[fd];
    return deviceTable[global_dev_no].dvwrite(process, buff, bufflen);
}

int di_ioctl(pcb* process, int fd, unsigned long command, va_list args){
    /* check that fd is valid and that device is open */
    if (fd < 0 || fd >= NUM_FD || (process->openedDvTable[fd] == -1)) {
        return DEV_ERROR;
    }

    /* perform command */
    int global_dev_no = process->openedDvTable[fd];
    int ret = deviceTable[global_dev_no].dvioctl(process, command, args);
    if (ret == DEV_DONE) {
        return DEV_DONE;
    } else {
        return DEV_ERROR;
    }
}


/* user.c : User processes
 */

#include <xeroskernel.h>
#include <xeroslib.h>
#include <kbd.h>
/*
void handler_exit(void *cntx) {
  char str[128];
  unsigned int me = sysgetpid();
  sprintf(str, "Process %03u received signal, calling sysstop now\n", me);
  sysputs(str);
  sysstop();
}

void handler_nothing(void *cntx) {
  char str[128];
  unsigned int me = sysgetpid();
  sprintf(str, "Process %03u received signal, does nothing\n", me);
  sysputs(str);
}

void busy( void ) {
  int myPid;
  char buff[100];
  int i;
  int count = 0;

  myPid = sysgetpid();

  for (i = 0; i < 10; i++) {
    sprintf(buff, "My pid is %d\n", myPid);
    sysputs(buff);
    if (myPid == 2 && count == 1) syskill(3);
    count++;
    sysyield();
  }
}

typedef void (*handler)(void*);

void root_testing_syssighandler(void) {
      int rc;
      void* ptr;
      handler old;

      // bad signal
      rc = syssighandler(-1, NULL, NULL);
      if (rc != -1 ) {
          kprintf("TEST 1: WRONG THIS SHOULD RETURN -1 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      kprintf("TEST 1 passed\n");

      // bad signal
      rc = syssighandler(32, NULL, NULL);
      if (rc != -1 ) {
          kprintf("TEST 2: WRONG THIS SHOULD RETURN -1 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      kprintf("TEST 2 passed\n");

      // good signal
      rc = syssighandler(0, handler_exit, &old);
      if (rc != 0) {
          kprintf("TEST 3: WRONG THIS SHOULD RETURN 0 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      if (old != NULL) {
          kprintf("TEST 3: WRONG OLD SHOULD BE NULL BUT IT IS %s INSTEAD.\n", old);
      }
      kprintf("TEST 3 passed\n");

      // bad signal
      rc = syssighandler(31, handler_exit, &old);
      if (rc != -1) {
          kprintf("TEST 4: WRONG THIS SHOULD RETURN -1 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      kprintf("TEST 4 passed\n");

      // good signal
      rc = syssighandler(30, handler_exit, &old);
      if (rc != 0) {
          kprintf("TEST 4.5: WRONG THIS SHOULD RETURN 0 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      if (old != NULL) {
          kprintf("TEST 4.5: WRONG OLD SHOULD BE NULL BUT IT IS %s INSTEAD.\n", old);
      }
      kprintf("TEST 4.5 passed\n");
      // empty invalid new handler
      ptr = kmalloc(sizeof(int) * 4);
      rc = syssighandler(20, (handler)ptr, &old);
      kfree(ptr);
      if (rc != -2) {
          kprintf("TEST 5: WRONG THIS SHOULD RETURN -2 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      kprintf("TEST 5 passed\n");

      // valid new handler
      rc = syssighandler(20, handler_exit, &old);
      if (rc != 0) {
          kprintf("TEST 6: WRONG THIS SHOULD RETURN 0 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      kprintf("TEST 6 passed\n");

      // valid old and new handlers
      rc = syssighandler(20, NULL, &old);
      if (rc != 0) {
          kprintf("TEST 7: WRONG THIS SHOULD RETURN 0 BUT IT RETURNED %d INSTEAD.\n", rc);
      }
      if ((unsigned int) old != (unsigned int)handler_exit) {
          kprintf("TEST 7: WRONG THIS SHOULD RETURN %d (old) BUT IT RETURNED %d INSTEAD.\n", handler_exit, old);
      }
      kprintf("TEST 7 passed\n");

      kprintf("Done\n");
}

void idle_wait_sig(void) {
  int rc;
  rc = syssighandler(20, handler_exit, NULL);
  if (rc != 0) {
      kprintf("IDLE_WAIT_SIG FAILED\n");
  }
  for(;;) {}
}

void loop_wait_sig(void) {
  unsigned int me;
  int rc;
  char str[128];

  me = sysgetpid();

  // Set signal handler
  rc = syssighandler(20, handler_exit, NULL);
  if (rc != 0){
      kprintf("LOOP WAIT SIG FAILED\n");
  }

  // Enter loop
  sprintf(str, "Process %03u entering infinite loop\n", me);
  sysputs(str);
  //while(1);
  sysputs("should never reach here\n");
}

void interrupted_by_sig(void) {
  int rc;

  // Set signal handler
  rc = syssighandler(20, handler_nothing, NULL);
  if (rc != 0) {
      kprintf("INTERRUPTED BY SIG HANDLER FAILED\n");
  }

  // block self
  //wait for the root process to terminate (block indefinitely)
  sysputs("waiting now\n");
  syswait(1);
  sysputs("this should not print out ..........");
}

void root_testing_syskill(void) {
  int rc;
  unsigned int pid, bg_pid;

  // Create an idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, PROC_STACK);

  // killing bad PID
  rc = syskill(-34, 20);
  if (rc != -512) {
      kprintf("TEST 1 EXPECTED -512, RETURNED %d\n", rc);
  }
  kprintf("TEST 1 passed\n");

  // Create a child process that only exits when receiving signal 20
  pid = syscreate(loop_wait_sig, PROC_STACK);

  // Wait for child process to set signal handler
  syssleep(5000);
  sysyield();
  sysyield();
  sysyield();
  sysyield();

  // Send an invalid signal
  rc = syskill(pid, -1);
  if (rc != -561) {
      kprintf("TEST 2 EXPECTED -561, RETURNED %d\n", rc);
  }
  kprintf("Test 2 passed\n");

  // Send signal 20 to child process
  rc = syskill(pid, 20);
  if (rc != 0) {
      kprintf("TEST 3 EXPECTED 0, RETURNED %d\n", rc);
  }
  kprintf("Test 3 passed\n");

  // Create a child process that blocks receiving, but it is not going to
  // receive any message because we will interrupt it with a signal
  char buf[128];
  pid = syscreate(interrupted_by_sig, PROC_STACK);

  // Give time for child process to block
  syssleep(1000);

  // Kill blocked process
  pcb* process = findPCB(pid);
  sprintf(buf, "State of blocked process before kill: %d\n", process->state);
  sysputs(buf);
  rc = syskill(pid, 20);
  if (rc != 0) {
      kprintf("TEST 4 EXPECTED 0, RETURNED %d\n", rc);
  }
  kprintf("TEST 4 passed.\n");
  kprintf("State of process: %d\n", process->state);
  kprintf("TEST 4: that pid ret is -99, its return: %d\n", process->ret);

  // kill background
  rc = syskill(bg_pid, 20);
  if (rc != 0) {
      kprintf("TEST 5 EXPECTED 0, RETURNED %d\n", rc);
  }
  kprintf("TEST 5 passed.");

  // try to kill idle process
  rc = syskill(0, 20);
  if (rc != -512) {
      kprintf("TEST 6 EXPECTED -512, RETURNED %d\n", rc);
  }
  kprintf("TEST 6 passed\n");

  kprintf("done\n");
}
void blocked_wait_sig(void) {
  int rc;

  // Set signal handler
  rc = syssighandler(20, handler_nothing, NULL);
  if (rc != 0) {
      kprintf("BAD SET UP\n");
  }

  // Wait for signal
  rc = syswait(1);

  if (rc != -99) {
      kprintf("PROCESS EXPECTED -99, RETURNED %d\n", rc);
  }
  else {
      kprintf("Good\n");
  }
}
void root_testing_syswait(void) {
 int rc;
 unsigned int pid, bg_pid;

  // Create idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, PROC_STACK);

  // Create a child process that does a sigwait before exiting
  pid = syscreate(blocked_wait_sig, PROC_STACK);

  // Wait for child process to set handler and call syssigwait
  syssleep(1000);

  pcb* proc = findPCB(pid);
  kprintf("state of process pid%d = %d\n", proc->pid, proc->state);

  // Send signal 20
  rc = syskill(pid, 20);
  if (rc != 0) {
      kprintf ("TEST 1 EXPECTED 0, RETURNED %d\n", rc);
  } else {
      kprintf("Test 1 passed.\n");
      kprintf("state of process pid%d = %d\n", proc->pid, proc->state);
      kprintf("process ret= %d \n", proc->ret);
  }

  rc = syskill(bg_pid, 20);
  if (rc != 0) {
      kprintf ("Bad kill", rc);
  }

  kprintf("done\n");
  sysstop();
}

void send_sig_lo(void* cntx) {
  unsigned int me;

  me = sysgetpid();
  syssleep(500);

  kprintf("sig_lo process %d exiting \n", me);
  syskill(me, 20);
}

void send_sig_mid(void* cntx) {
  unsigned int me;

  me = sysgetpid();
  char str[128];
  syssleep(500);
  sprintf(str, "sig_mid process %d exiting\n", me);
  sysputs(str);
}

void send_sig_hi(void* cntx) {
  unsigned int me;

  me = sysgetpid();
  char str[128];
  syssleep(500);
  sprintf(str, "sig_hi process %d exiting\n", me);
  sysputs(str);
}

void stack_sigtramp(void) {
  int rc;

  // Setup signal handlers
  rc = syssighandler(20, handler_exit, NULL);
  if (rc != 0){
      kprintf("20:bad setup\n");
  }
  rc = syssighandler(10, send_sig_lo, NULL);
  if (rc != 0){
      kprintf("10:bad setup\n");
  }
  rc = syssighandler(15, send_sig_mid, NULL);
  if (rc != 0){
      kprintf("15:bad setup\n");
  }
  rc = syssighandler(30, send_sig_hi, NULL);
  if (rc != 0){
      kprintf("30:bad setup\n");
  }

  for(;;);
}

void root_testing_prioritysigs(void) {
  unsigned int pid, bg_pid;

  int rc;

  // Create idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, PROC_STACK);

  // Create a child process that accepts signals of different priorities
  pid = syscreate(stack_sigtramp, PROC_STACK);

  // Wait for child to set up handlers
  syssleep(1000);

  // Send signals in increasing priority so sigtramp stacks
  rc = syskill(pid, 10);
  if (rc != 0) {
      kprintf("TEST 1 EXPECT 0, RETURN %d\n", rc);
  } else {
      kprintf("Test 1 passed.\n");
  }
  //syssleep(1000);

  rc = syskill(pid, 15);
  if (rc != 0) {
      kprintf("TEST 2 EXPECT 0, RETURN %d\n", rc);
  } else {
      kprintf("Test 2 passed.\n");
  }
  //syssleep(1000);

  rc = syskill(pid, 30);
  if (rc != 0) {
      kprintf("TEST 3 EXPECT 0, RETURN %d\n", rc);
  } else {
      kprintf("Test 3 passed.\n");
  }

  kprintf("now killinh bg_pid\n");
  rc = syskill(bg_pid, 20);

  // Give child processes some time to finish first
  syssleep(5000);

  kprintf("DONE DONE DONE \n");
}

void root_testing_sysopen(void) {
      int rc, fd;

  // out of bounds
  rc = sysopen(-1);
  if (rc != -1) {
      kprintf("TEST 1 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 1 passed.\n");
  }

  // out of bounds
  rc = sysopen(4);
  if (rc != -1) {
      kprintf("TEST 2 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 2 passed.\n");
  }

  // good open with device num 0
  fd = sysopen(0);
  if (fd != 0) {
      kprintf("TEST 3 EXPECTED 0, RETURN %d\n", fd);
  } else {
      kprintf("Test 3 passed.\n");
  }

  // try to open same device again
  rc = sysopen(0);
  if (rc != -1) {
      kprintf("TEST 4 EXPECTED -1, RETURN %d\n", fd);
  } else {
      kprintf("Test 4 passed.\n");
  }

  // a device is already opened - cannot open
  rc = sysopen(1);
  if (rc != -1) {
      kprintf("TEST 5 EXPECTED -1, RETURN %d\n", fd);
  } else {
      kprintf("Test 5 passed.\n");
  }

  // close the open device
  rc = sysclose(fd);
  if (rc != 0) {
      kprintf("Device 0 didn't close properly\n");
  } else {
      kprintf("Device 0 closed\n");
  }

  //open another device with device num 1
  fd = sysopen(1);
  if (fd != 1) {
      kprintf("TEST 6 EXPECTED 1, RETURN %d\n", fd);
  } else {
      kprintf("Test 6 passed.\n");
  }

  rc = sysclose(fd);
  if (rc != 0) {
      kprintf("Device didn't close properly\n");
  } else {
      kprintf("Device closed\n");
  }
}

void root_testing_sysclose(void) {
  int rc, fd;

  fd = 0;
  rc = sysclose(fd);
  if (rc != -1) {
      kprintf("TEST 1 EXPECTED -1, RETURNED %d\n");
  } else {
      kprintf("Test 1 passed\n");
  }

  fd = -1;
  rc = sysclose(fd);
  if (rc != -1) {
      kprintf("TEST 2 EXPECTED -1, RETURNED %d\n");
  } else {
      kprintf("Test 2 passed\n");
  }

  fd = 4;
  rc = sysclose(fd);
  if (rc != -1) {
      kprintf("TEST 3 EXPECTED -1, RETURNED %d\n");
  } else {
      kprintf("Test 3 passed\n");
  }

  fd = sysopen(0);
  if (fd != 0) {
      kprintf("Prob with setup \n");
  }

  rc = sysclose(fd);
  if (rc != 0) {
      kprintf("TEST 4 EXPECTED 0, RETURNED %d\n");
  } else {
      kprintf("Test 4 passed\n");
  }

  rc = sysclose(fd);
  if (rc != -1) {
      kprintf("TEST 5 EXPECTED -1, RETURNED %d\n");
  } else {
      kprintf("Test 5 passed\n");
  }

  fd = sysopen(1);
  if (fd < 0 || fd >= 3) {
      kprintf("prob with setup, fd = %d\n", fd);
  }

  rc = sysclose(fd);
  if (rc != 0) {
      kprintf("TEST 6 EXPECTED 0, RETURNED %d\n");
  } else {
      kprintf("Test 6 passed\n");
  }

  kprintf("done\n");
}

void root_testing_syswrite(void) {
  int rc, fd;
  char buf[128];

  // invalid fd
  fd = -1;
  rc = syswrite(fd, buf, 128);
  if (rc != -1) {
      kprintf("TEST 1 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 1 passed\n");
  }

  // invalid fd
  fd = 4;
  rc = syswrite(fd, buf, 128);
  if (rc != -1) {
      kprintf("TEST 2 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 2 passed\n");
  }

  // no device open
  fd = 0;
  rc = syswrite(fd, buf, 128);
  if (rc != -1) {
      kprintf("TEST 3 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 3 passed\n");
  }

  fd = sysopen(0);
  if (fd != 0) {
  }

  // writes are not supported to keyboard
  rc = syswrite(fd, buf, 128);
  if (rc != -1) {
      kprintf("TEST 4 EXPECTED -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 4 passed\n");
  }

  rc = sysclose(fd);
  kprintf("done\n");
}

void root_testing_sysioctl(void) {
  int rc, fd, fd1;
  char buf[128];
  char buf2[128];
  int cmd;

  // no device open
  fd = 0;
  rc = sysioctl(fd, 30);
  if (rc != -1) {
      kprintf("TEST 1 EXPECT -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 1 passed \n");
  }

  //open device 0
  fd = sysopen(0);

  // no such command
  rc = sysioctl(fd, 30);
  if (rc != -1) {
      kprintf("TEST 2 EXPECT -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 2 passed \n");
  }

  // change the eof
  cmd = 53;
  rc = sysioctl(fd, cmd, '!');
  if (rc != 0) {
      kprintf("TEST 3 EXPECT 0, RETURN %d\n", rc);
  }
  if (eof != '!') {
      kprintf("TEST 3 EXPECT eof %d, RETURN eof %d\n", '!', eof);
  } else {
      kprintf("Test 3 passed.\n");
  }

  // set kbd_buf to read from
  kbd_buf[0] = '1';
  kbd_buf[1] = '2';
  kbd_buf[2] = '3';
  kbd_buf[3] = '!';
  kbd_len = 4;

  //read to make sure eof is working
  rc = sysread(fd, buf, 128);
  if (rc != 3) {
      kprintf("TEST 4 EXPECT READ 3, RETURN %d\n", rc);
  } else  {
      kprintf("Test 4 passed\n");
  }
  buf[rc] = 0;
  if (strcmp(buf, "123") != 0) {
      kprintf("TEST 4 EXPECT STRCMP 0, BUF RETURN: %s\n", buf);
  }
  //read again should return 0 because reached eof already
  rc = sysread(fd, buf, 128);
  if (rc != 0) {
      kprintf ("TEST 4 EXPECT 0 FROM SYSREAD, RETURN %d\n", rc);
  }

  //close device
  rc = sysclose(fd);
  if (rc != 0) {
      kprintf ("problem with close\n");
  }

  kbd_buf[(kbd_index+kbd_len)%KBD_BUF_SIZE] = '2';
  kbd_buf[(kbd_index+kbd_len+1)%KBD_BUF_SIZE] = '3';
  kbd_buf[(kbd_index+kbd_len+2)%KBD_BUF_SIZE] = '4';
  kbd_buf[(kbd_index+kbd_len+2)%KBD_BUF_SIZE] = EOF;
//  kbd_buf[1] = '3';
//  kbd_buf[2] = '4';
//  kbd_buf[3] = EOF;
  kbd_len = 4;

  //create non-echoing device
  fd1 = sysopen(0);
  if (fd1 != 0) {
      kprintf("problem opening device 0\n");
  }

  // turn echo on
  cmd = 56;
  rc = sysioctl(fd1,cmd);
  if (rc != 0) {
      kprintf("TEST 5 EXPECT 0, RETURN %d\n", rc);
  }
  if (!echo) {
      kprintf("TEST 5 EXPECT echo 1, RETURN echo %d\n", echo);
  }

  kprintf("This is the eof: %d\n", eof);
  rc = sysread(fd1, buf2, 128);
  if (rc != 3) {
      kprintf("TEST 5 EXPECT READ 3, RETURN %d\n", rc);
  }
  buf2[rc] = 0;
  if (strcmp(buf2, "234") != 0) {
      kprintf("TEST 5 EXPECT STRCMP 0, BUF RETURN: %s\n", buf);
  }
  rc = sysclose(fd1);
  if (rc != 0) {
      kprintf ("problem with close\n");
  }

  //open device with echoing
 fd = sysopen(1);
 kprintf("before ioctl, echo was:%d\n", echo);
 // turn off echo
  cmd = 55;
  rc = sysioctl(fd, cmd);
  if (rc != 0) {
      kprintf("TEST 6 EXPECT 0, RETURN %d\n", rc);
  }
  if (echo) {
      kprintf("TEST 6 EXPECT echo 0, RETURN echo %d\n", echo);
  } else {
      kprintf("Test 6 passed.\n");
  }

  // no such command
  rc = sysioctl(fd, -3);
  if (rc != -1) {
      kprintf("TEST 7 EXPECT -1, RETURN %d\n", rc);
  } else {
      kprintf("Test 7 passed \n");
  }

  rc = sysclose(fd);
  if (rc != 0) {
      kprintf ("problem with close\n");
  }


  kprintf("done\n");
}

void root_testing_sysread(void) {
  int rc, fd;
  char buf[4];
  char buf1[4];
  char buf2[4];
  int bg_pid;

  //open a device
  fd = sysopen(0);

  // set up keyboard buffer first time
  kbd_buf[0] = 'a';
  kbd_buf[1] = 'b';
  kbd_buf[2] = 'c';
  kbd_buf[3] = 'd';
  kbd_len = 2;

  // test non blocking sysread
  rc = sysread(fd, buf, 2);
  if (rc != 2) {
      kprintf("TEST 1 EXPECTED 2, RETURNED %d\n", rc);
  }
  buf[rc] = 0;
  if (strcmp(buf, "ab") != 0){
      kprintf("TEST 1 EXPECTED STRCMP 0. RETURNED SOMETHING ELSE\n");
  } else {
      kprintf("Test 1 passed\n");
  }
  kprintf("TEST 1 after kbd_index:%d\n", kbd_index);
  kprintf("TEST 1 after kbd_len:%d\n", kbd_len);
  //close device
  rc = sysclose(fd);
  if (rc != 0) {
      kprintf("prob closing device \n");
  }

  bg_pid = syscreate(idle_wait_sig, 128);

  //open device
  fd = sysopen(1);

  // test blocking sysread
  // set up keyboard buffer first time
  kprintf("adding to kbdbuf, kbd_len:%d\n", kbd_len);
  kbd_buf[0] = 'a';
  kbd_buf[1] = 'b';
  kbd_buf[2] = 'c';
  kbd_buf[3] = 'd';
  kbd_index = 0;
  kbd_len = 4;
  rc = sysread(fd, buf1, 128);
  if (rc < 0) {
      kprintf("TEST 2 SYSREAD EXPECTED RC >0, RETURNED %d\n", rc);
  } else {
      kprintf ("Test 2 passed \n");
  }
  buf1[rc] = 0;
  kprintf("TEST 2 buf: %s\n", buf1);
  kprintf("TEST 2 after read:%d\n", p_read);
  kprintf("TEST 2 after kbd_index:%d\n", kbd_index);
  kprintf("TEST 2 after kbd_len:%d\n", kbd_len);

  //kill background process with sig 2
  syskill(bg_pid, 20);

  sysclose(fd);

  //set up kbd_buf second time
  kbd_buf[0] = 'a';
  kbd_buf[1] = 'b';
  kbd_buf[2] = 'c';
  kbd_buf[3] = EOF;
  kbd_index = 0;
  kbd_len = 4;
  //open device
  fd = sysopen(0);

  // testing eof
  kprintf("TEST 3 before read:%d\n", p_read);
  kprintf("TEST 3 before kbd_index:%d\n", kbd_index);
  kprintf("TEST 3 before kbd_len:%d\n", kbd_len);
  rc = sysread(fd, buf2, 5);
  if (rc != 3) {
      kprintf("TEST 3 EXPECT 3, RETURN %d\n", rc);
  }
  buf2[rc] = 0;
  if (strcmp(buf2, "abc") != 0) {
      kprintf("TEST 3 STRCMP EXPECT 0, RETURNED SOMETHING ELSE:%s\n", buf2);
  }else {
      kprintf("TEST 3 passed.\n");
  }
  kprintf("TEST 3 buf: %s\n", buf2);
  kprintf("TEST 3 after read:%d\n", p_read);
  kprintf("TEST 3 after kbd_index:%d\n", kbd_index);
  kprintf("TEST 3 after kbd_len:%d\n", kbd_len);

  rc = sysread(fd, buf2, 4);
  if (rc != 0) {
      kprintf("TEST 4 EXPECTED 0, RETURNED %d\n", rc);
  }else {
      kprintf("Test 4 passed\n");
  }

  //close device
  sysclose(fd);
}

void test_sig(void) {
    kprintf("signaling\n");
    syskill(1,20);
    kprintf("should not go here\n");
}

void test_sig2(void) {
    kprintf("signaling 2\n");
    syskill(1,21);
    kprintf("should not go here\n");
}

void test_sig3(void) {
    kprintf("signaling 2\n");
    syskill(1,22);
    kprintf("should not go here\n");
}
void sig2(void) {
    kprintf("BOOM\n");
}
void sig(void) {
    kprintf("BOOM BOM\n");
}
void sig3(void) {
    kprintf("BOOM BOM WHAT\n");
}

void root_test(void) {
    syssighandler(20, sig, NULL);
    syssighandler(21, sig2, NULL);
    syssighandler(22, sig3, NULL);
    syscreate(test_sig, PROC_STACK);
    syscreate(test_sig2, PROC_STACK);
    syscreate(test_sig3, PROC_STACK);
    sysyield();
    kprintf("it's happneing");
}


void root_testing_signal(void) {
    int pid = syscreate(test_sig, PROC_STACK);
    syssighandler(20, sig, NULL);
    sysyield();
    syskill(pid, 31);
    pcb* test = findPCB(pid);
    if (test->state == STATE_STOPPED) {
        kprintf("YES, STOP SIGNAL IS WORKING\n");
    }else {
        kprintf("NOPE, NOT WORKING. try yielding\n");
    }
    sysyield();
    if (test->state == STATE_STOPPED) {
        kprintf("YES, STOP SIGNAL IS WORKING\n");
    }else {
        kprintf("NOPE, NOT WORKING.\n");
    }
}
void test(void) {
    char buf[128];
    int fd = sysopen(1);
    sysread(fd, buf, 10);
    sysclose(fd);
}
*/

/* ****************************************************************************************** start of assignment root*/
void t_func(void) {
    //process prints T on new line every 10 seconds
    while (TRUE) {
        sysputs("T\n");
        //sleep for 10 seconds
        syssleep(10000);
    }
}

int shellPID;
int time;

int count = 0;
void alarmHandler(void* cntx) {
    //kprintf("alarmHandler:%d\n", alarmHandler);
    kprintf("ALARM ALARM ALARM\n");
}

void a_func(void) {
   // kprintf("sleeping for %d ms\n", time);
    syssleep(time);
    int result = signal(shellPID, 15);
    //kprintf("a_func Result of signal:%d\n", result);
}

void shell(void) {
    char command_buf[128];
    int result;
    int fd;
    int* PID;
    int PID_index = 0;

    shellPID = sysgetpid();

    //open a device
    //kprintf("shell open:%d, echo:%d, eof_flag:%d, p_read:%d, p_len:%d, p_buf:%s, kbd_len:%d, kbd_index:%d\n", open, echo, eof_flag, p_read, p_len, p_buf, kbd_len, kbd_index);
    fd = sysopen(1);
    int me = sysgetpid();
    //kprintf("me:%d\n", me);
    //kprintf("open:%d, echo:%d, eof_flag:%d, p_read:%d, p_len:%d, p_buf:%s, kbd_len:%d, kbd_index:%d\n", open, echo, eof_flag, p_read, p_len, p_buf, kbd_len, kbd_index);

    while (TRUE) {
        // reset kbd_buf
        for (int i = 0; i < 4; i++) {
            kbd_buf[i] = 0;
        }
        //reset kbd_len, reset kbd_index
        kbd_len = 0;
        kbd_index = 0;
        // reset p_read, p_buf, p_len, eof_flag
        eof_flag = 0;
        p_read = 0;
        p_buf = NULL;
        p_len = 0;
        // reset command_buf
        memset(command_buf, 0, sizeof command_buf);

        //print prompt >
        sysputs("\n>");

        //kprintf("command_buf:%s,%d\n", command_buf,command_buf);
        //kprintf("fd for sysread:%d\n", fd);
        // sysread command
        result = sysread(fd, command_buf, 128);

        //kprintf("THIS IS COMMAND_BUF:%s,%d\n", command_buf, command_buf);

        // test for ps
        int buf_len = strlen(command_buf);
        if ((strcmp(command_buf, "ps") == 0) || (strcmp(command_buf, "ps &") == 0) || (strcmp(command_buf,"ps&") == 0)) {
            processStatuses psTab;
            int procs;
            char* status;
            char buf[128];
            procs = sysgetcputimes(&psTab);
            kprintf("PID    State             Runtime\n");
            for(int j = 0; j <= procs; j++) {
                switch (psTab.status[j]) {
                    case (STATE_STOPPED):
                        status = "stopped";
                        break;
                    case (STATE_READY):
                        status = "ready";
                        break;
                    case (STATE_SLEEP):
                        status = "sleeping";
                        break;
                    case (STATE_RUNNING):
                        status = "running";
                        break;
                    case (STATE_WAITING):
                        status = "waiting";
                        break;
                    case (STATE_BLOCKED_READING):
                        status = "reading";
                        break;
                    case (STATE_BLOCKED_WRITING):
                        status = "writing";
                        break;
                    default:
                        kprintf("no such state to change\n");
                }
              sprintf(buf, "%4d    %4s    %10d\n", psTab.pid[j], status, psTab.cpuTime[j]);
              kprintf(buf);
            }
        // print T every 10 seconds and sleep
        } else if ((strcmp(command_buf, "t") == 0) || (strcmp(command_buf, "t&") == 0) || (strcmp(command_buf, "t &") == 0)) {
            if (command_buf[result -2] == '&') {
                PID[PID_index++] = syscreate(t_func, PROC_STACK);
                // go back to step 1
            } else {

                PID  = syscreate(t_func, PROC_STACK);
                syswait(PID);
            }
        //test for current EOF
        } else if (command_buf[0] == eof ) {
            break;
        // test for ex
        } else if (strcmp(command_buf, "ex") == 0) {
            break;
        // test for k
        } else if (command_buf[0] == 'k') {
            // get the PID
            char PID_buf[10];
            int q;
            for (q = 2; q < (result -1); q++) {
                PID_buf[q-2] = command_buf[q];
            }
            PID_buf[q] = NULL;
            //set char to int
            int kill_PID = atoi(PID_buf);

            // find process
            pcb* process = findPCB(kill_PID);
            if (process->state != STATE_STOPPED){
                syskill(kill_PID, 31);
            } else {
                kprintf("No such process\n");
            }
        } else if (command_buf[0] == 'a' ) {
            //get the num of ticks
            char tick_buf[10];
            // assuming that command is sent as 'a #\n'
            int q;
            for (q = 2; q < (result -1); q++) {
                tick_buf[q-2] = command_buf[q];
            }
            tick_buf[q] = '\0';
            //set char to int
            int tick = atoi(tick_buf);
            //set time in milliseconds
            time = tick * 10;
            //kprintf("tick_buf:%s, tick:%d, time:%d\n", tick_buf, tick, time);

            // register alarmHandler
            void(**old) (void*);
            //kprintf("alarmHandler:%d\n", alarmHandler);
            // handler is disabled in deliver_signal after it has been delivered
            syssighandler(15, alarmHandler, old);

            if (command_buf[result - 2] == '&') {
                // create alarm
                PID[PID_index++] = syscreate(a_func, PROC_STACK);
                // go back to step 1
            } else {
                // create alarm
                int afunc = syscreate(a_func, PROC_STACK);
                syswait(afunc);
            }
        } else {
            kprintf("Command not found\n");
           // kprintf("command_buf: %s, %d", command_buf, command_buf);
        }
    }
    // close device
    int rc = sysclose(fd);
}

        /* ignore for nowbu
        for (int z = PID_index; z >= 0; z--) {
            if (findPCB(PID[z])->state == STATE_STOPPED){
                PID[z] = NULL;
            }
            else {
                PID_index = z;
                break;
            }
        }

*/

void root (void) {
    char user_buf[128];
    char pw_buf[128];
    int fd;

    while (TRUE) {
        while (TRUE) {
            // print banner
            sysputs("Welcome to Xeros - an experimental OS\n");
            // open keyboard with echo
            kprintf("root open:%d, echo:%d, eof_flag:%d, p_read:%d, p_len:%d, p_buf:%s, kbd_len:%d, kbd_index:%d\n", open, echo, eof_flag, p_read, p_len, p_buf, kbd_len, kbd_index);
            fd = sysopen(1);
            kprintf("fd:%d\n",fd);
            // print 'username:'
            sysputs("Username: ");
            // read username
            sysread(fd, user_buf, 6);
            //turn echoing off
            sysioctl(fd, 55);
            //print 'password'
            sysputs("Password: ");
            //reset flags before re-reading and read password
            // reset eof flag, chars read, process buf, buflen
            eof_flag = 0;
            p_read = 0;
            //p_buf = NULL;
            p_len = 0;
            p_buf = NULL;
            sysread(fd, pw_buf, 16);
            //close keyboard
            int rc = sysclose(fd);
            if (rc != 0) {
                kprintf("DEVICE DID NOT CLOSE:%d\n", rc);
            }

            // verify user and password
            if ((strcmp(user_buf,"cs415") == 0) && (strcmp(pw_buf, "EveryonegetsanA") == 0)) {
           // if ((strcmp(user_buf,"a") == 0) && (strcmp(pw_buf, "a") == 0)) {
                kprintf("VERIFICATION SUCCESSFUL\n");

                break;
            }
            //kprintf("user_buf:%s, pw_buf:%s\n", user_buf, pw_buf);
            //If not verified, go to back step 1
        }

        //Create shell program
        shellPID = syscreate(shell, PROC_STACK);
        kprintf("shell program created\n");
        while (1) {}
        //Wait for shell program to exit
        syswait(shellPID);


        // Return to step 1
    }
}

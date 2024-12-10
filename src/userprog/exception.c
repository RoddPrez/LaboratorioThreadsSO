#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static void handle_stack_growth (void *fault_addr);

/* Registers handlers for interrupts that can be caused by user
   programs. */
void exception_init(void) {
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions. */
  intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int(5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction. */
  intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int(7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int(19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void) {
  printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame *f) {
  /* This interrupt is one (probably) caused by a user process.
     Kill the user process. */
  switch (f->cs) {
  case SEL_UCSEG:
    printf("%s: dying due to interrupt %#04x (%s).\n",
           thread_name(), f->vec_no, intr_name(f->vec_no));
    intr_dump_frame(f);
    thread_exit(); 

  case SEL_KCSEG:
    intr_dump_frame(f);
    PANIC("Kernel bug - unexpected interrupt in kernel"); 

  default:
    printf("Interrupt %#04x (%s) in unknown segment %04x\n",
           f->vec_no, intr_name(f->vec_no), f->cs);
    thread_exit();
  }
}

/* Page fault handler. */
static void page_fault(struct intr_frame *f) {
  bool not_present;
  bool write;
  bool user;
  void *fault_addr;

  /* Obtain faulting address. */
  asm("movl %%cr2, %0" : "=r"(fault_addr));

  /* Turn interrupts back on. */
  intr_enable();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* Handle page fault caused by stack growth or other page faults. */
  if (is_user_vaddr(fault_addr)) {
    handle_stack_growth(fault_addr);
  } else {
    printf("Page fault at %p: %s error %s page in %s context.\n",
           fault_addr,
           not_present ? "not present" : "rights violation",
           write ? "writing" : "reading",
           user ? "user" : "kernel");
    kill(f);
  }
}

/* Handle stack growth when a page fault occurs in the stack area. */
static void handle_stack_growth(void *fault_addr) {
  struct thread *current_thread = thread_current();
  uint8_t *fault_page = (uint8_t *)pg_round_down(fault_addr);
  
  /* Check if the faulting address is within the allowed stack growth area. */
  if (fault_page >= current_thread->stack_bottom - PGSIZE &&
      fault_page < current_thread->stack_bottom) {
    void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL) {
      if (!pagedir_set_page(current_thread->pagedir, fault_page, kpage, true)) {
        palloc_free_page(kpage);
      }
    } else {
      sys_exit(-1);
    }
  } else {
    sys_exit(-1);
  }
}

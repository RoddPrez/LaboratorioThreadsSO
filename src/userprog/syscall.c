#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "lib/string.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *f);
static void syscall_write (int fd, const void *buffer, unsigned size);

void syscall_init (void) {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f) {
    uint32_t *args = (uint32_t *) f->esp;
    switch (args[0]) {
        case SYS_WRITE:
            syscall_write(args[1], (const void *) args[2], args[3]);
            break;
        default:
            break;
    }
}

static void syscall_write (int fd, const void *buffer, unsigned size) {
    if (fd == 1) {
        putbuf(buffer, size);
    } else {
        struct file *file = process_get_file(fd);
        if (file != NULL) {
            file_write(file, buffer, size);
        }
    }
}

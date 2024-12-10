#include "userprog/pagedir.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "vm/page.h"
#include "lib/kernel/list.h"

static bool lazy_load_page(void *upage);

bool pagedir_get_page (void *upage) {
    struct thread *t = thread_current();
    struct page *p = page_find(t, upage);
    if (p != NULL && p->loaded) {
        return true;
    }
    return lazy_load_page(upage);
}

static bool lazy_load_page(void *upage) {
    struct thread *t = thread_current();
    uint8_t *kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL) {
        return false;
    }

    bool success = false;
    if (process_load_page(upage, kpage)) {
        success = pagedir_set_page(t->pagedir, upage, kpage, true);
    }
    
    if (!success) {
        palloc_free_page(kpage);
    }

    return success;
}
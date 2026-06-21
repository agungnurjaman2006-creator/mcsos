#include "mcsos_thread.h"

static uintptr_t align_down_uintptr(uintptr_t value, uintptr_t alignment) {
    return value & ~(alignment - 1u);
}

static int valid_thread_object(const mcsos_thread_t *thread) {
    return thread != (const mcsos_thread_t *)0 && thread->magic == MCSOS_THREAD_MAGIC;
}

static void zero_context(mcsos_context_t *context) {
    context->rsp = 0;
    context->rbp = 0;
    context->rbx = 0;
    context->r12 = 0;
    context->r13 = 0;
    context->r14 = 0;
    context->r15 = 0;
    context->rip = 0;
}

/*
 * Kontrak panduan M9 mendefinisikan mcsos_thread_trampoline() sebagai
 * placeholder hlt loop murni, tanpa pernah memanggil thread->entry(thread->arg).
 * Ini ketidaklengkapan kontrak: contoh integrasi kernel (Langkah 7) jelas
 * mengharapkan entry() benar-benar berjalan (mis. demo_thread_a mencetak log
 * berulang), tapi tidak ada mekanisme bagi trampoline (dipanggil tanpa
 * argumen lewat jmp, bukan call) untuk tahu thread mana dirinya.
 *
 * Perbaikan: g_active_sched menyimpan pointer scheduler aktif, diisi oleh
 * mcsos_scheduler_init. Trampoline membaca sched->current (thread yang baru
 * saja "menjadi dirinya sendiri" tepat setelah context switch menyelesaikan
 * jmp ke rip ini) untuk memperoleh entry dan arg, lalu memanggilnya. Jika
 * entry tidak ada (boot thread) atau entry kembali (return), trampoline
 * jatuh ke hlt loop sebagai fallback aman -- mempertahankan maksud asli
 * placeholder kontrak sebagai jaring pengaman, bukan menghapusnya.
 */
static mcsos_scheduler_t *g_active_sched = (mcsos_scheduler_t *)0;

void mcsos_thread_trampoline(void) {
    if (g_active_sched != (mcsos_scheduler_t *)0) {
        mcsos_thread_t *self = g_active_sched->current;
        if (valid_thread_object(self) && self->entry != (mcsos_thread_entry_t)0) {
            self->entry(self->arg);
        }
    }
    for (;;) {
#if defined(__x86_64__)
        __asm__ volatile("hlt");
#else
        __builtin_trap();
#endif
    }
}

int mcsos_scheduler_init(mcsos_scheduler_t *sched, mcsos_thread_t *boot_thread) {
    if (sched == (mcsos_scheduler_t *)0 || boot_thread == (mcsos_thread_t *)0) {
        return MCSOS_SCHED_EINVAL;
    }
    boot_thread->magic = MCSOS_THREAD_MAGIC;
    boot_thread->id = 0;
    boot_thread->name = "boot";
    boot_thread->state = MCSOS_THREAD_RUNNING;
    boot_thread->entry = (mcsos_thread_entry_t)0;
    boot_thread->arg = (void *)0;
    boot_thread->stack_base = (uint8_t *)0;
    boot_thread->stack_size = 0;
    boot_thread->next = (mcsos_thread_t *)0;
    boot_thread->switches = 0;
    boot_thread->ticks = 0;
    boot_thread->exit_code = 0;
    zero_context(&boot_thread->context);

    sched->current = boot_thread;
    sched->idle = boot_thread;
    sched->ready_head = (mcsos_thread_t *)0;
    sched->ready_tail = (mcsos_thread_t *)0;
    sched->next_id = 1;
    sched->runnable_count = 0;
    sched->context_switches = 0;
    sched->ticks = 0;
    sched->initialized = 1;

    g_active_sched = sched;
    return MCSOS_SCHED_OK;
}

int mcsos_thread_prepare(mcsos_thread_t *thread,
                         const char *name,
                         mcsos_thread_entry_t entry,
                         void *arg,
                         void *stack_base,
                         size_t stack_size,
                         uint64_t id) {
    if (thread == (mcsos_thread_t *)0 || entry == (mcsos_thread_entry_t)0 || stack_base == (void *)0) {
        return MCSOS_SCHED_EINVAL;
    }
    if (stack_size < MCSOS_MIN_KERNEL_STACK) {
        return MCSOS_SCHED_ESTACK;
    }
    uintptr_t low = (uintptr_t)stack_base;
    uintptr_t high = low + (uintptr_t)stack_size;
    if (high <= low) {
        return MCSOS_SCHED_ESTACK;
    }
    uintptr_t top = align_down_uintptr(high, MCSOS_STACK_ALIGN);
    if (top <= low + 128u) {
        return MCSOS_SCHED_ESTACK;
    }
    top -= sizeof(uint64_t);
    *((uint64_t *)top) = UINT64_C(0);

    thread->magic = MCSOS_THREAD_MAGIC;
    thread->id = id;
    thread->name = name;
    thread->state = MCSOS_THREAD_NEW;
    zero_context(&thread->context);
    thread->context.rsp = (uint64_t)top;
    thread->context.rip = (uint64_t)(uintptr_t)mcsos_thread_trampoline;
    thread->entry = entry;
    thread->arg = arg;
    thread->stack_base = (uint8_t *)stack_base;
    thread->stack_size = stack_size;
    thread->next = (mcsos_thread_t *)0;
    thread->switches = 0;
    thread->ticks = 0;
    thread->exit_code = 0;
    return MCSOS_SCHED_OK;
}

int mcsos_sched_enqueue(mcsos_scheduler_t *sched, mcsos_thread_t *thread) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(thread)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (thread->state != MCSOS_THREAD_NEW && thread->state != MCSOS_THREAD_READY && thread->state != MCSOS_THREAD_BLOCKED) {
        return MCSOS_SCHED_ESTATE;
    }
    thread->state = MCSOS_THREAD_READY;
    thread->next = (mcsos_thread_t *)0;
    if (sched->ready_tail == (mcsos_thread_t *)0) {
        sched->ready_head = thread;
        sched->ready_tail = thread;
    } else {
        sched->ready_tail->next = thread;
        sched->ready_tail = thread;
    }
    sched->runnable_count++;
    return MCSOS_SCHED_OK;
}

mcsos_thread_t *mcsos_sched_pick_next(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0) {
        return (mcsos_thread_t *)0;
    }
    mcsos_thread_t *thread = sched->ready_head;
    if (thread == (mcsos_thread_t *)0) {
        return sched->idle;
    }
    sched->ready_head = thread->next;
    if (sched->ready_head == (mcsos_thread_t *)0) {
        sched->ready_tail = (mcsos_thread_t *)0;
    }
    thread->next = (mcsos_thread_t *)0;
    if (sched->runnable_count > 0u) {
        sched->runnable_count--;
    }
    return thread;
}

int mcsos_sched_yield(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    mcsos_thread_t *old_thread = sched->current;
    mcsos_thread_t *next_thread = mcsos_sched_pick_next(sched);
    if (!valid_thread_object(next_thread)) {
        return MCSOS_SCHED_ECORRUPT;
    }
    if (next_thread == old_thread) {
        old_thread->state = MCSOS_THREAD_RUNNING;
        return MCSOS_SCHED_OK;
    }
    if (old_thread->state == MCSOS_THREAD_RUNNING && old_thread != sched->idle) {
        old_thread->state = MCSOS_THREAD_READY;
        int rc = mcsos_sched_enqueue(sched, old_thread);
        if (rc != MCSOS_SCHED_OK) {
            return rc;
        }
    }
    next_thread->state = MCSOS_THREAD_RUNNING;
    sched->current = next_thread;
    old_thread->switches++;
    next_thread->switches++;
    sched->context_switches++;
#if !defined(MCSOS_HOST_TEST)
    mcsos_context_switch(&old_thread->context, &next_thread->context);
#endif
    return MCSOS_SCHED_OK;
}

int mcsos_sched_tick(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    sched->ticks++;
    sched->current->ticks++;
    return MCSOS_SCHED_OK;
}

int mcsos_thread_block_current(mcsos_scheduler_t *sched) {
    if (sched == (mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (sched->current == sched->idle) {
        return MCSOS_SCHED_ESTATE;
    }
    sched->current->state = MCSOS_THREAD_BLOCKED;
    return mcsos_sched_yield(sched);
}

int mcsos_thread_mark_ready(mcsos_scheduler_t *sched, mcsos_thread_t *thread) {
    if (!valid_thread_object(thread)) {
        return MCSOS_SCHED_EINVAL;
    }
    if (thread->state != MCSOS_THREAD_BLOCKED) {
        return MCSOS_SCHED_ESTATE;
    }
    return mcsos_sched_enqueue(sched, thread);
}

size_t mcsos_sched_ready_count(const mcsos_scheduler_t *sched) {
    if (sched == (const mcsos_scheduler_t *)0 || sched->initialized == 0) {
        return 0u;
    }
    size_t count = 0u;
    const mcsos_thread_t *cursor = sched->ready_head;
    while (cursor != (const mcsos_thread_t *)0) {
        count++;
        cursor = cursor->next;
    }
    return count;
}

int mcsos_sched_validate(const mcsos_scheduler_t *sched) {
    if (sched == (const mcsos_scheduler_t *)0 || sched->initialized == 0 || !valid_thread_object(sched->current)) {
        return MCSOS_SCHED_EINVAL;
    }
    size_t count = 0u;
    const mcsos_thread_t *cursor = sched->ready_head;
    const mcsos_thread_t *last = (const mcsos_thread_t *)0;
    while (cursor != (const mcsos_thread_t *)0) {
        if (!valid_thread_object(cursor) || cursor->state != MCSOS_THREAD_READY) {
            return MCSOS_SCHED_ECORRUPT;
        }
        if (cursor == sched->current) {
            return MCSOS_SCHED_ECORRUPT;
        }
        last = cursor;
        cursor = cursor->next;
        count++;
        if (count > sched->runnable_count + 1u) {
            return MCSOS_SCHED_ECORRUPT;
        }
    }
    if (last != sched->ready_tail) {
        return MCSOS_SCHED_ECORRUPT;
    }
    if (count != (size_t)sched->runnable_count) {
        return MCSOS_SCHED_ECORRUPT;
    }
    return MCSOS_SCHED_OK;
}

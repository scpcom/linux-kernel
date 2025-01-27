/**
 * Copyright (C) Arm Limited 2010-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "gator.h"

#include <linux/hardirq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/swap.h>
#include <linux/workqueue.h>
#include <trace/events/kmem.h>

#define USE_THREAD defined(CONFIG_PREEMPT_RT_FULL)

/*
 * Handle rename of global_page_state "c41f012ade0b95b0a6e25c7150673e0554736165 mm: rename global_page_state to global_zone_page_state"
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define GLOBAL_ZONE_PAGE_STATE(item)    global_page_state(item)
#else
#define GLOBAL_ZONE_PAGE_STATE(item)    global_zone_page_state(item)
#endif

enum {
    MEMINFO_MEMFREE,
    MEMINFO_MEMUSED,
    MEMINFO_BUFFERRAM,
    MEMINFO_CACHED,
    MEMINFO_SLAB,
    MEMINFO_TOTAL,
};

enum {
    PROC_SIZE,
    PROC_SHARE,
    PROC_TEXT,
    PROC_DATA,
    PROC_COUNT,
};

static const char * const meminfo_names[] = {
    "Linux_meminfo_memfree",
    "Linux_meminfo_memused",
    "Linux_meminfo_bufferram",
    "Linux_meminfo_cached",
    "Linux_meminfo_slab",
};

static const char * const proc_names[] = {
    "Linux_proc_statm_size",
    "Linux_proc_statm_share",
    "Linux_proc_statm_text",
    "Linux_proc_statm_data",
};

static bool meminfo_global_enabled;
static ulong meminfo_enabled[MEMINFO_TOTAL];
static ulong meminfo_keys[MEMINFO_TOTAL];
static long long meminfo_buffer[2 * (MEMINFO_TOTAL + 2)];
static int meminfo_length;
static bool new_data_avail;

static bool proc_global_enabled;
static ulong proc_enabled[PROC_COUNT];
static ulong proc_keys[PROC_COUNT];
static DEFINE_PER_CPU(long long, proc_buffer[2 * (PROC_COUNT + 3)]);

static void do_read(void);

#if USE_THREAD

static int gator_meminfo_func(void *data);
static bool gator_meminfo_run;
/* Initialize semaphore unlocked to initialize memory values */
static DEFINE_SEMAPHORE(gator_meminfo_sem);

static void notify(void)
{
    up(&gator_meminfo_sem);
}

#else

static unsigned int mem_event;
static void wq_sched_handler(struct work_struct *wsptr);
static DECLARE_WORK(work, wq_sched_handler);
static struct timer_list meminfo_wake_up_timer;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
static void meminfo_wake_up_handler(struct timer_list *t);
#else
static void meminfo_wake_up_handler(unsigned long unused_data);
#endif

static void notify(void)
{
    mem_event++;
}

#endif

GATOR_DEFINE_PROBE(mm_page_free, TP_PROTO(struct page *page, unsigned int order))
{
    notify();
}

GATOR_DEFINE_PROBE(mm_page_free_batched, TP_PROTO(struct page *page, int cold))
{
    notify();
}

GATOR_DEFINE_PROBE(mm_page_alloc, TP_PROTO(struct page *page, unsigned int order, gfp_t gfp_flags, int migratetype))
{
    notify();
}

static int gator_events_meminfo_create_files(struct super_block *sb, struct dentry *root)
{
    struct dentry *dir;
    int i;

    for (i = 0; i < MEMINFO_TOTAL; i++) {
        dir = gatorfs_mkdir(sb, root, meminfo_names[i]);
        if (!dir)
            return -1;
        gatorfs_create_ulong(sb, dir, "enabled", &meminfo_enabled[i]);
        gatorfs_create_ro_ulong(sb, dir, "key", &meminfo_keys[i]);
    }

    for (i = 0; i < PROC_COUNT; ++i) {
        dir = gatorfs_mkdir(sb, root, proc_names[i]);
        if (!dir)
            return -1;
        gatorfs_create_ulong(sb, dir, "enabled", &proc_enabled[i]);
        gatorfs_create_ro_ulong(sb, dir, "key", &proc_keys[i]);
    }

    return 0;
}

static int gator_events_meminfo_start(void)
{
    int i;

    new_data_avail = false;
    meminfo_global_enabled = 0;
    for (i = 0; i < MEMINFO_TOTAL; i++) {
        if (meminfo_enabled[i]) {
            meminfo_global_enabled = 1;
            break;
        }
    }

    proc_global_enabled = 0;
    for (i = 0; i < PROC_COUNT; ++i) {
        if (proc_enabled[i]) {
            proc_global_enabled = 1;
            break;
        }
    }
    if (meminfo_enabled[MEMINFO_MEMUSED])
        proc_global_enabled = 1;

    if (meminfo_global_enabled == 0)
        return 0;

    if (GATOR_REGISTER_TRACE(mm_page_free))
        goto mm_page_free_exit;
    if (GATOR_REGISTER_TRACE(mm_page_free_batched))
        goto mm_page_free_batched_exit;
    if (GATOR_REGISTER_TRACE(mm_page_alloc))
        goto mm_page_alloc_exit;

    do_read();
#if USE_THREAD
    /* Start worker thread */
    gator_meminfo_run = true;
    /* Since the mutex starts unlocked, memory values will be initialized */
    if (IS_ERR(kthread_run(gator_meminfo_func, NULL, "gator_meminfo")))
        goto kthread_run_exit;
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
    timer_setup(&meminfo_wake_up_timer, meminfo_wake_up_handler, TIMER_DEFERRABLE);
#else
    setup_deferrable_timer_on_stack(&meminfo_wake_up_timer, meminfo_wake_up_handler, 0);
#endif
#endif

    return 0;

#if USE_THREAD
kthread_run_exit:
    GATOR_UNREGISTER_TRACE(mm_page_alloc);
#endif
mm_page_alloc_exit:
    GATOR_UNREGISTER_TRACE(mm_page_free_batched);
mm_page_free_batched_exit:
    GATOR_UNREGISTER_TRACE(mm_page_free);
mm_page_free_exit:
    return -1;
}

static void gator_events_meminfo_stop(void)
{
    if (meminfo_global_enabled) {
        GATOR_UNREGISTER_TRACE(mm_page_free);
        GATOR_UNREGISTER_TRACE(mm_page_free_batched);
        GATOR_UNREGISTER_TRACE(mm_page_alloc);

#if USE_THREAD
        /* Stop worker thread */
        gator_meminfo_run = false;
        up(&gator_meminfo_sem);
#else
        del_timer_sync(&meminfo_wake_up_timer);
#endif
    }
}

static void do_read(void)
{
    struct sysinfo info;
    int i, len;
    unsigned long long value;

    meminfo_length = len = 0;

    si_meminfo(&info);
    for (i = 0; i < MEMINFO_TOTAL; i++) {
        if (meminfo_enabled[i]) {
            switch (i) {
            case MEMINFO_MEMFREE:
                value = info.freeram * PAGE_SIZE;
                break;
            case MEMINFO_MEMUSED:
                /* pid -1 means system wide */
                meminfo_buffer[len++] = 1;
                meminfo_buffer[len++] = -1;
                /* Emit value */
                meminfo_buffer[len++] = meminfo_keys[MEMINFO_MEMUSED];
                meminfo_buffer[len++] = (info.totalram - info.freeram) * PAGE_SIZE;
                /* Clear pid */
                meminfo_buffer[len++] = 1;
                meminfo_buffer[len++] = 0;
                continue;
            case MEMINFO_BUFFERRAM:
                value = info.bufferram * PAGE_SIZE;
                break;
            case MEMINFO_CACHED:
                // total_swapcache_pages is not exported so the result is slightly different, but hopefully not too much
                value = (GLOBAL_ZONE_PAGE_STATE(NR_FILE_PAGES) /*- total_swapcache_pages()*/ - info.bufferram) * PAGE_SIZE;
                break;
            case MEMINFO_SLAB:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
                value = (GLOBAL_ZONE_PAGE_STATE(NR_SLAB_RECLAIMABLE_B) + GLOBAL_ZONE_PAGE_STATE(NR_SLAB_UNRECLAIMABLE_B)) * PAGE_SIZE;
#else
                value = (GLOBAL_ZONE_PAGE_STATE(NR_SLAB_RECLAIMABLE) + GLOBAL_ZONE_PAGE_STATE(NR_SLAB_UNRECLAIMABLE)) * PAGE_SIZE;
#endif
                break;
            default:
                value = 0;
                break;
            }
            meminfo_buffer[len++] = meminfo_keys[i];
            meminfo_buffer[len++] = value;
        }
    }

    meminfo_length = len;
    new_data_avail = true;
}

#if USE_THREAD

static int gator_meminfo_func(void *data)
{
    for (;;) {
        if (down_killable(&gator_meminfo_sem))
            break;

        /* Eat up any pending events */
        while (!down_trylock(&gator_meminfo_sem))
            ;

        if (!gator_meminfo_run)
            break;

        do_read();
    }

    return 0;
}

#else

/* Must be run in process context as the kernel function si_meminfo() can sleep */
static void wq_sched_handler(struct work_struct *wsptr)
{
    do_read();
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
static void meminfo_wake_up_handler(struct timer_list *t)
#else
static void meminfo_wake_up_handler(unsigned long unused_data)
#endif
{
    /* had to delay scheduling work as attempting to schedule work during the context switch is illegal in kernel versions 3.5 and greater */
    schedule_work(&work);
}

#endif

static int gator_events_meminfo_read(long long **buffer, bool sched_switch)
{
#if !USE_THREAD
    static unsigned int last_mem_event;
#endif

    if (!on_primary_core() || !meminfo_global_enabled)
        return 0;

#if !USE_THREAD
    if (last_mem_event != mem_event) {
        last_mem_event = mem_event;
        mod_timer(&meminfo_wake_up_timer, jiffies + 1);
    }
#endif

    if (!new_data_avail)
        return 0;

    new_data_avail = false;

    if (buffer)
        *buffer = meminfo_buffer;

    return meminfo_length;
}

static int gator_events_meminfo_read_proc(long long **buffer, struct task_struct *task)
{
    struct mm_struct *mm;
    u64 share = 0;
    int i;
    long long value;
    int len = 0;
    int cpu = get_physical_cpu();
    long long *buf = per_cpu(proc_buffer, cpu);

    if (!proc_global_enabled)
        return 0;

    /* Collect the memory stats of the process instead of the thread */
    if (task->group_leader != NULL)
        task = task->group_leader;

    /* get_task_mm/mmput is not needed in this context because the task and it's mm are required as part of the sched_switch */
    mm = task->mm;
    if (mm == NULL)
        return 0;

    /* Derived from task_statm in fs/proc/task_mmu.c */
    if (meminfo_enabled[MEMINFO_MEMUSED] || proc_enabled[PROC_SHARE]) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
        share = get_mm_counter(mm, MM_FILEPAGES);
#else
        share = get_mm_counter(mm, MM_FILEPAGES) + get_mm_counter(mm, MM_SHMEMPAGES);
#endif
    }

    /* key of 1 indicates a pid */
    buf[len++] = 1;
    buf[len++] = task->pid;

    for (i = 0; i < PROC_COUNT; ++i) {
        if (proc_enabled[i]) {
            switch (i) {
            case PROC_SIZE:
                value = mm->total_vm;
                break;
            case PROC_SHARE:
                value = share;
                break;
            case PROC_TEXT:
                value = (PAGE_ALIGN(mm->end_code) - (mm->start_code & PAGE_MASK)) >> PAGE_SHIFT;
                break;
            case PROC_DATA:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
                value = mm->total_vm - mm->shared_vm;
#else
                value = mm->total_vm - mm->stack_vm;
#endif
                break;
            }

            buf[len++] = proc_keys[i];
            buf[len++] = value * PAGE_SIZE;
        }
    }

    if (meminfo_enabled[MEMINFO_MEMUSED]) {
        value = share + get_mm_counter(mm, MM_ANONPAGES);
        /* Send resident for this pid */
        buf[len++] = meminfo_keys[MEMINFO_MEMUSED];
        buf[len++] = value * PAGE_SIZE;
    }

    /* Clear pid */
    buf[len++] = 1;
    buf[len++] = 0;

    if (buffer)
        *buffer = buf;

    return len;
}

static struct gator_interface gator_events_meminfo_interface = {
    .name = "meminfo",
    .create_files = gator_events_meminfo_create_files,
    .start = gator_events_meminfo_start,
    .stop = gator_events_meminfo_stop,
    .read64 = gator_events_meminfo_read,
    .read_proc = gator_events_meminfo_read_proc,
};

int gator_events_meminfo_init(void)
{
    int i;

    meminfo_global_enabled = 0;
    for (i = 0; i < MEMINFO_TOTAL; i++) {
        meminfo_enabled[i] = 0;
        meminfo_keys[i] = gator_events_get_key();
    }

    proc_global_enabled = 0;
    for (i = 0; i < PROC_COUNT; ++i) {
        proc_enabled[i] = 0;
        proc_keys[i] = gator_events_get_key();
    }

    return gator_events_install(&gator_events_meminfo_interface);
}

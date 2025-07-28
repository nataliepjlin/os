#include "kernel/types.h"
#include "user/user.h"
#include "user/list.h"
#include "user/threads.h"
#include "user/threads_sched.h"
#include <limits.h>
#define NULL 0

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
         __typeof__ (b) _b = (b); \
      _a < _b ? _a : _b; })

int find_min_release_time(struct list_head *release_queue){
    int min_release_time = -1;
    struct release_queue_entry *entry = NULL;
    list_for_each_entry(entry, release_queue, thread_list){
        if(min_release_time == -1 || entry->release_time < min_release_time){
            min_release_time = entry->release_time;
        }
    }
    return min_release_time;
}

/* default scheduling algorithm */
#ifdef THREAD_SCHEDULER_DEFAULT
struct threads_sched_result schedule_default(struct threads_sched_args args)
{
    struct thread *thread_with_smallest_id = NULL;
    struct thread *th = NULL;
    list_for_each_entry(th, args.run_queue, thread_list) {
        if (thread_with_smallest_id == NULL || th->ID < thread_with_smallest_id->ID)
            thread_with_smallest_id = th;
    }

    struct threads_sched_result r;
    if (thread_with_smallest_id != NULL) {
        r.scheduled_thread_list_member = &thread_with_smallest_id->thread_list;
        r.allocated_time = thread_with_smallest_id->remaining_time;
    } else {
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = 1;
    }

    return r;
}
#endif

/* MP3 Part 1 - Non-Real-Time Scheduling */

// HRRN
#ifdef THREAD_SCHEDULER_HRRN
struct threads_sched_result schedule_hrrn(struct threads_sched_args args)
{
    struct threads_sched_result r;
    // TO DO: Highest Response Ratio Next: Response Ratio = (Waiting Time + Burst Time) / Burst Time
    if(list_empty(args.run_queue)){
        int min_release_time = find_min_release_time(args.release_queue);
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = (min_release_time == -1) ? 1 : min_release_time - args.current_time;
        return r;
    }
    struct thread *hrrn_thread = NULL, *th = NULL;
    int hrrn_wait_time = -1;
    list_for_each_entry(th, args.run_queue, thread_list){
        int cur_wait_time = args.current_time - th->arrival_time;
        if(hrrn_thread == NULL
        || hrrn_wait_time * th->processing_time < cur_wait_time * hrrn_thread->processing_time
        || (hrrn_wait_time * th->processing_time == cur_wait_time * hrrn_thread->processing_time && th->ID < hrrn_thread->ID)){
            hrrn_thread = th;
            hrrn_wait_time = cur_wait_time;
        }
    }
    r.scheduled_thread_list_member = &(hrrn_thread->thread_list);
    r.allocated_time = hrrn_thread->processing_time;//  without forcibly interrupting currently running threads
    return r;
}
#endif

#ifdef THREAD_SCHEDULER_PRIORITY_RR
// priority Round-Robin(RR)
struct threads_sched_result schedule_priority_rr(struct threads_sched_args args) 
{
    struct threads_sched_result r;
    // TO DO: Priority-based Round Robin
    if(list_empty(args.run_queue)){
        int min_release_time = find_min_release_time(args.release_queue);
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = (min_release_time == -1) ? 1 : min_release_time - args.current_time;
        return r;
    }

    struct thread *th = NULL;
    int highest_priority = INT_MAX;
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->priority < highest_priority) highest_priority = th->priority;// lower values indicating higher priority
    }

    static int prev_priority = -1;
    static int prev_id = -1;
    if(highest_priority == prev_priority){// on the same level, use rr
        struct thread *next_thread = NULL, *th = NULL;
        int cur_min_id = INT_MAX, has_same_priority = -1;
        th = NULL;
        list_for_each_entry(th, args.run_queue, thread_list){
            if(th->priority == highest_priority){
                if(th->ID > prev_id && cur_min_id > th->ID){
                    next_thread = th;
                    cur_min_id = th->ID;
                }
                has_same_priority++;
            }
        }
        if(next_thread == NULL){//need wrap around
            th = NULL;
            list_for_each_entry(th, args.run_queue, thread_list){
                if(th->priority == highest_priority && cur_min_id > th->ID){
                    next_thread = th;
                    cur_min_id = th->ID;
                }
            }
        }
        prev_id = cur_min_id;
        r.scheduled_thread_list_member = &(next_thread->thread_list);
        r.allocated_time = (has_same_priority > 0) ? min(args.time_quantum, next_thread->remaining_time) : next_thread->remaining_time;//take release_queue into account??
        return r;
    }

    struct thread *next_thread = NULL;
    th = NULL;
    int cur_min_id = INT_MAX;
    int has_same_priority = -1;
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->priority == highest_priority){
            if(cur_min_id > th->ID){
                next_thread = th;
                cur_min_id = th->ID;
            }
            has_same_priority++;
        }
    }
    prev_priority = highest_priority;
    prev_id = cur_min_id;
    r.scheduled_thread_list_member = &(next_thread->thread_list);
    r.allocated_time = (has_same_priority > 0) ? min(args.time_quantum, next_thread->remaining_time) : next_thread->remaining_time;//take release_queue into account??

    return r;
}
#endif

/* MP3 Part 2 - Real-Time Scheduling*/

#if defined(THREAD_SCHEDULER_EDF_CBS) || defined(THREAD_SCHEDULER_DM)
static struct thread *__check_deadline_miss(struct list_head *run_queue, int current_time)
{
    struct thread *th = NULL;
    struct thread *thread_missing_deadline = NULL;
    list_for_each_entry(th, run_queue, thread_list) {
        if (th->current_deadline <= current_time) {
            if (thread_missing_deadline == NULL)
                thread_missing_deadline = th;
            else if (th->ID < thread_missing_deadline->ID)
                thread_missing_deadline = th;
        }
    }
    return thread_missing_deadline;
}
#endif

#ifdef THREAD_SCHEDULER_DM
/* Deadline-Monotonic Scheduling */
// static int __dm_thread_cmp(struct thread *a, struct thread *b)
// {
//     //To DO
// }

struct threads_sched_result schedule_dm(struct threads_sched_args args)
{
    struct threads_sched_result r;

    // first check if there is any thread has missed its current deadline
    // TO DO
    struct thread *missing_ddl = __check_deadline_miss(args.run_queue, args.current_time);
    if(missing_ddl != NULL){
        // If there is a thread that has already missed its current deadline, set scheduled_thread_list_member to the thread_list member of that thread and set allocated_time to 0
        r.scheduled_thread_list_member = &(missing_ddl->thread_list);
        r.allocated_time = 0;
        return r;
    }

    // shortest deadline is assigned highest priority
    struct thread *th = NULL, *shortest_ddl = NULL;
    list_for_each_entry(th, args.run_queue, thread_list){
        if(shortest_ddl == NULL 
        || th->deadline < shortest_ddl->deadline
        || (th->deadline == shortest_ddl->deadline && th->ID < shortest_ddl->ID)){
            shortest_ddl = th;
        }
    }

    // handle the case where run queue is empty
    // TO DO
    if(shortest_ddl == NULL){
        int min_release_time = find_min_release_time(args.release_queue);
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = (min_release_time == -1) ? 1 : min_release_time - args.current_time;
        return r;
    }
    
    int allocated_time = min(shortest_ddl->remaining_time, shortest_ddl->current_deadline - args.current_time);
    // check if there's going to be a release before the allocated time
    struct release_queue_entry *entry = NULL;
    int min_release_time = -1;// the time that the thrd with min ddl be released
    list_for_each_entry(entry, args.release_queue, thread_list){
        if(entry->thrd->deadline < shortest_ddl->deadline
        || (entry->thrd->deadline == shortest_ddl->deadline && entry->thrd->ID < shortest_ddl->ID)
        ){
            if(min_release_time == -1 || entry->release_time < min_release_time)
                min_release_time = entry->release_time;
        }
    }
    r.scheduled_thread_list_member = &(shortest_ddl->thread_list);
    r.allocated_time = (min_release_time == -1) ? allocated_time : min(allocated_time, min_release_time - args.current_time);

    return r;
}
#endif


#ifdef THREAD_SCHEDULER_EDF_CBS
// EDF with CBS comparation
// static int __edf_thread_cmp(struct thread *a, struct thread *b)
// {
//     // TO DO
// }
//  EDF_CBS scheduler: Earliest Deadline First (EDF) with Constant Bandwidth Server (CBS)
struct threads_sched_result schedule_edf_cbs(struct threads_sched_args args)
{
    struct threads_sched_result r;

    // notify the throttle task
    // TO DO
    struct thread *th;
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->cbs.is_hard_rt
        || th->cbs.remaining_budget > 0)
            continue;

        if(args.current_time < th->current_deadline){
            // printf("[debug] th %d is throttled\n", th->ID);
            th->cbs.is_throttled = 1;
        }
        else{
            // printf("[debug] th %d is UNthrottled\n", th->ID);
            th->cbs.is_throttled = 0;
            th->current_deadline += th->period;
            th->cbs.remaining_budget = th->cbs.budget;
        }
    }

    // first check if there is any thread has missed its current deadline
    // TO DO
    struct thread *missing_ddl = __check_deadline_miss(args.run_queue, args.current_time);
    if(missing_ddl != NULL){
        // If there is a thread that has already missed its current deadline, set scheduled_thread_list_member to the thread_list member of that thread and set allocated_time to 0
        if(missing_ddl->cbs.is_hard_rt){
            r.scheduled_thread_list_member = &(missing_ddl->thread_list);
            r.allocated_time = 0;
            return r;
        }
    }
    // shortest absolute deadline is assigned highest priority
    struct thread *shortest_abs_ddl = NULL;
    int need_loop = 1;
    while(need_loop){
        need_loop = 0;
        list_for_each_entry(th, args.run_queue, thread_list){
            // printf("[debug] looping through th %d. arrival = %d, remain_time = %d, remain_budget = %d, current_ddl = %d\n", th->ID, th->arrival_time, th->remaining_time, th->cbs.remaining_budget, th->current_deadline);
            if(th->cbs.is_throttled) continue;
            if(shortest_abs_ddl == NULL 
            || th->current_deadline < shortest_abs_ddl->current_deadline
            || (th->current_deadline == shortest_abs_ddl->current_deadline && th->ID < shortest_abs_ddl->ID)){
                shortest_abs_ddl = th;
            }
        }
        if(shortest_abs_ddl != NULL && !shortest_abs_ddl->cbs.is_hard_rt){// soft, check bandwidth constraint
            // Remaining CBS Budget Time/Time Until CBS Deadline > CBS Budget Time/Period
            if(shortest_abs_ddl->cbs.remaining_budget * shortest_abs_ddl->period > shortest_abs_ddl->cbs.budget * (shortest_abs_ddl->current_deadline - args.current_time)){
                need_loop = 1;
                // New CBS Deadline = Current Time + Period
                // printf("[debug] th %d is extended with new ddl\n", shortest_abs_ddl->ID);
                shortest_abs_ddl->current_deadline = args.current_time + shortest_abs_ddl->period;
                shortest_abs_ddl->cbs.remaining_budget =  shortest_abs_ddl->cbs.budget;
                shortest_abs_ddl = NULL;
            }
        }
    }

    // handle the case where run queue is empty
    // TO DO
    if(shortest_abs_ddl == NULL){// can all threads are throttled?
        int min_release_time = find_min_release_time(args.release_queue);
        list_for_each_entry(th, args.run_queue, thread_list){
            if(th->cbs.is_throttled
            && (min_release_time == -1|| th->current_deadline < min_release_time))
                min_release_time = th->current_deadline; 
        }
        r.scheduled_thread_list_member = args.run_queue;
        r.allocated_time = (min_release_time == -1) ? 1 : min_release_time - args.current_time;
        return r;
    }

    int allocated_time = (shortest_abs_ddl->cbs.is_hard_rt) ? min(shortest_abs_ddl->remaining_time, shortest_abs_ddl->current_deadline - args.current_time) : min(min(shortest_abs_ddl->cbs.remaining_budget, shortest_abs_ddl->remaining_time), shortest_abs_ddl->current_deadline - args.current_time);
    // check if there's going to be a release before the allocated time
    struct release_queue_entry *entry = NULL;
    int min_release_time = -1;// the time that the thrd with min ddl be released
    list_for_each_entry(entry, args.release_queue, thread_list){
        if(entry->thrd->current_deadline < shortest_abs_ddl->current_deadline
        || (entry->thrd->current_deadline == shortest_abs_ddl->current_deadline && entry->thrd->ID < shortest_abs_ddl->ID)
        ){
            if(min_release_time == -1 || entry->release_time < min_release_time)
                min_release_time = entry->release_time;
        }
    }
    list_for_each_entry(th, args.run_queue, thread_list){
        if(th->cbs.is_throttled
        && (th->current_deadline < shortest_abs_ddl->current_deadline
        || (th->current_deadline == shortest_abs_ddl->current_deadline && th->ID < shortest_abs_ddl->ID))){
            if(min_release_time == -1 || th->current_deadline < min_release_time)
                min_release_time = th->current_deadline;
        }
    }
    // printf("th %d is scheduled, hard: %d, arrival = %d, ddl = %d, allocated time = %d, remaining = %d, min_release_time = %d\n", shortest_abs_ddl->ID, shortest_abs_ddl->cbs.is_hard_rt, shortest_abs_ddl->arrival_time, shortest_abs_ddl->current_deadline, allocated_time, shortest_abs_ddl->remaining_time, min_release_time);
    r.scheduled_thread_list_member = &(shortest_abs_ddl->thread_list);
    r.allocated_time = (min_release_time == -1) ? allocated_time : min(allocated_time, min_release_time - args.current_time);

    return r;
}
#endif
#include "kernel/types.h"
#include "user/setjmp.h"
#include "user/threads.h"
#include "user/user.h"
#define NULL 0


static struct thread* current_thread = NULL;
static int id = 1;

//the below 2 jmp buffer will be used for main function and thread context switching
static jmp_buf env_st; 
// static jmp_buf env_tmp;  

struct thread *get_current_thread() {
    return current_thread;
}

struct thread *thread_create(void (*f)(void *), void *arg){
    struct thread *t = (struct thread*) malloc(sizeof(struct thread));
    unsigned long new_stack_p;
    unsigned long new_stack;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    new_stack_p = new_stack +0x100*8-0x2*8;
    t->fp = f; 
    t->arg = arg;
    t->ID  = id;
    t->buf_set = 0;
    t->stack = (void*) new_stack; //points to the beginning of allocated stack memory for the thread.
    t->stack_p = (void*) new_stack_p; //points to the current execution part of the thread.
    id++;

    // part 2
    t->suspended = 0;
    t->sig_handler[0] = NULL_FUNC;
    t->sig_handler[1] = NULL_FUNC;
    t->signo = -1;
    t->handler_buf_set = 0;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    new_stack_p = new_stack +0x100*8-0x2*8;
    t->handler_stack = (void*)new_stack;
    t->handler_stack_p = (void*)new_stack_p;
    return t;
}


void thread_add_runqueue(struct thread *t){
    if(current_thread == NULL){
        current_thread = t;
        current_thread->next = current_thread;
        current_thread->previous = current_thread;
    }else{
        //TO DO
        t->sig_handler[0] = current_thread->sig_handler[0];
        t->sig_handler[1] = current_thread->sig_handler[1];
        struct thread *tail = current_thread->previous;
        tail->next = t;
        t->next = current_thread;
        current_thread->previous = t;
        t->previous = tail;
    }
}
void thread_yield(void){
    //TO DO
    if(current_thread->signo == -1){
        if(setjmp(current_thread->env) == 0){
            current_thread->buf_set = 1;
            schedule();
            dispatch();
        }
        else{
            // Execution will resume here after longjmp. No need to do anything, just return to the calling place
        }
    }
    else{
        if(setjmp(current_thread->handler_env) == 0){
            current_thread->handler_buf_set = 1;
            schedule();
            dispatch();
        }
        else{
            // current_thread->signo = -1;
            // printf("thread %d's signo reset\n", current_thread->ID);
        }
    }
}

void dispatch(void){
    //TO DO     
    while(current_thread->suspended){
        // printf("thread %d is scheduled but suspended\n", current_thread->ID);
        current_thread = current_thread->next;
    }
    if(current_thread->signo != -1){
        if(current_thread->sig_handler[current_thread->signo] == NULL_FUNC) thread_exit();
        else{
            if(!current_thread->handler_buf_set){
                if(setjmp(current_thread->handler_env) == 0){
                    current_thread->handler_buf_set = 1;
                    current_thread->handler_env[0].sp = (unsigned long)current_thread->handler_stack_p;
                    longjmp(current_thread->handler_env, 1);
                }
                current_thread->sig_handler[current_thread->signo](current_thread->signo);
            }
            else longjmp(current_thread->handler_env, 1);
        }
    }
    if(!current_thread->buf_set){
        if(setjmp(current_thread->env) == 0){
            current_thread->buf_set = 1;
            current_thread->env[0].sp = (unsigned long)current_thread->stack_p;// moving the stack pointer sp to the allocated stack of the thread
            longjmp(current_thread->env, 1);
        }
        current_thread->fp(current_thread->arg);
        thread_exit();
    }
    else longjmp(current_thread->env, 1); 
}

//schedule will follow the rule of FIFO
void schedule(void){
    current_thread = current_thread->next;
    
    //Part 2: TO DO
    while(current_thread->suspended) {
        current_thread = current_thread->next;
    };
    
}

void thread_exit(void){
    if(current_thread->next != current_thread){
        //TO DO
        struct thread *next = current_thread->next, *prev = current_thread->previous;
        next->previous = prev;
        prev->next = next;
        free(current_thread->stack);
        free(current_thread->handler_stack);
        free(current_thread);
        current_thread = next;
        dispatch();
    }
    else{
        free(current_thread->stack);
        free(current_thread->handler_stack);
        free(current_thread);
        current_thread = NULL;
        longjmp(env_st, 1);
    }
}
void thread_start_threading(void){
    //TO DO
    while(current_thread != NULL){
        if(setjmp(env_st) == 0) dispatch();
    }
}

//PART 2
void thread_register_handler(int signo, void (*handler)(int)){
    // Signals will only be sent from a parent thread to its child threads. A parent thread will not sent signals to the same child thread more than once.
    current_thread->sig_handler[signo] = handler;
}

void thread_kill(struct thread *t, int signo){
    //TO DO
    t->signo = signo;
}

void thread_suspend(struct thread *t) {
    //TO DO
    t->suspended = 1;
    if(current_thread == t){
        //If the thread suspended itself, you may need to call thread_yield()
        thread_yield();
    }
}

void thread_resume(struct thread *t) {
    //TO DO
    t->suspended = 0;
}


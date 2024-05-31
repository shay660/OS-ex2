#include "uthreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <setjmp.h>

#define READY 0
#define RUNNING 1
#define BLOCKED 2

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7


typedef struct Thread {
  int tid;
  int state;
  int n_quantum;
  thread_entry_point entry_point;
  char stack[STACK_SIZE];
  sigjmp_buf env;
} Thread;

GQueue *ready_queue = NULL;
Thread* threads[MAX_THREAD_NUM];
int current_thread = -1;

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

int uthread_init(int quantum_usecs) {
  ready_queue = g_queue_new();
  // TODO: Implement this function
  return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
  if (entry_point == NULL)
  {
    printf("thread library error: null entry point\n");
    return -1;
  }

  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    if (threads[i] == NULL)
    {
      Thread* new_thread = malloc(sizeof(Thread));
      if (new_thread == NULL)
      {
        return -1;
      }
      new_thread->tid = i;
      new_thread->state = READY;
      new_thread->n_quantum = 0;
      new_thread->entry_point = entry_point;
      sigsetjmp(new_thread->env, 1);
      address_t sp = (address_t) new_thread->stack + STACK_SIZE - sizeof
          (address_t);
      address_t pc = (address_t) entry_point;
      (new_thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
      (new_thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
      sigemptyset(&new_thread->env->__saved_mask);
      threads[i] = new_thread;
      g_queue_push_tail(ready_queue, new_thread);
      return i;
    }
  }
  return -1;
}


void running_next_thread()
{
  Thread* next_thread = g_queue_pop_head(ready_queue);
  g_queue_push_tail(ready_queue, threads[current_thread]);
  current_thread = next_thread->tid;
  next_thread->state = RUNNING;
  siglongjmp(next_thread->env, 1);
}

int uthread_terminate(int tid) {
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == NULL)
  {
    printf("thread library error: invalid tid\n");
    return -1;
  }
    if (tid == 0)
    {
        exit(0);
    }
    if (threads[tid]->state == RUNNING)
    {
      running_next_thread();
    }

    if (threads[tid]->state == READY)
    {
      g_queue_remove(ready_queue, threads[tid]);
    }

    free(threads[tid]);
    threads[tid] = NULL;
  return 0;
}

int uthread_block(int tid) {
  // TODO: Implement this function
  return 0;
}

int uthread_resume(int tid) {
  // TODO: Implement this function
  return 0;
}

int uthread_sleep(int num_quantums) {
  // TODO: Implement this function
  return 0;
}

int uthread_get_tid() {
  // TODO: Implement this function
  return 0;
}

int uthread_get_total_quantums() {
  // TODO: Implement this function
  return 0;
}

int uthread_get_quantums(int tid) {
  // TODO: Implement this function
  return 0;
}
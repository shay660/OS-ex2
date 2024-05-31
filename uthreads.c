#include "uthreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#definr READY 0
#define RUNNING 1
#define BLOCKED 2

typedef struct Thread {
  int tid;
  int state;
  int n_quantum;
  thread_entry_point entry_point;
  char stack[STACK_SIZE];
  sigjmp_buf env;
} Thread;

GQueue *ready_queue = g_queue_new();
Thread threads[MAX_THREAD_NUM];
int current_thread = -1;

int uthread_init(int quantum_usecs) {
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
      (new_thread->env->__jmpbuf)[JB_SP] = translate_address
          (new_thread->stack + STACK_SIZE - sizeof(address_t));
      (new_thread->env->__jmpbuf)[JB_PC] = translate_address(entry_point);
      sigemptyset(&new_thread->env->__saved_mask);
      threads[i] = new_thread;
      g_queue_push_tail(ready_queue, new_thread);
      return i;
    }
  }
  return -1;
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

void running_next_thread() {
  if (g_queue_is_empty(ready_queue))
  {
    printf("thread library error: no more threads to run\n");
    exit(0);
  }
  Thread* next_thread = g_queue_pop_head(ready_queue);
  next_thread->state = RUNNING;
  current_thread = next_thread->tid;
  next_thread->n_quantum++;
  siglongjmp(next_thread->env, 1);
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
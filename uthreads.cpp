#include "uthreads.h"
#include <stdlib.h>
#include <stdio.h>
//#include <glib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include "queue"
#include <cstdio>
#include <cerrno>

#define READY 0
#define RUNNING 1
#define BLOCKED 2

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

int TOTAL_QUANTUMS = 0;

typedef struct Thread {
    int tid;
    int state;
    int n_quantum;
    float wake_up_time;
    thread_entry_point entry_point;
    char stack[STACK_SIZE];
    sigjmp_buf env;
} Thread;

//GQueue *ready_queue = NULL;
std::queue<Thread *> ready_queue;
Thread *threads[MAX_THREAD_NUM];
int current_thread = -1;
struct itimerval timer;

void scheduler ();

void remove_from_queue (int tid);
/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

bool is_valid_tid (int tid)
{
  if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr)
    {
      printf ("thread library error: invalid tid\n");
      return false;
    }
  return true;
}

// Signal handler for SIGVTALRM
void run_next_thread (int sig)
{
//  if (!ready_queue.empty ())
//    {
//      printf ("thread library error: no ready threads\n");
//      exit (1);


  // Save the current thread's context
  if (sigsetjmp(threads[current_thread]->env, 1) == 0)
    {
      // Select the next thread to run
      if (threads[current_thread] != nullptr)
        {
          ready_queue.push (threads[current_thread]);
          threads[current_thread]->state = READY;
        }
      Thread *next_thread = ready_queue.front ();
      ready_queue.pop ();
      current_thread = next_thread->tid;
      next_thread->state = RUNNING;
      next_thread->n_quantum++;
      // Restore the next thread's context
      siglongjmp (next_thread->env, 1);
    }
//    }
}

int uthread_init (int quantum_usecs)
{
//  ready_queue = g_queue_new();
//  if (ready_queue == NULL)
//  {
//    printf("system error: %s\n", strerror(errno));
//    exit(1);
//  }
  for (auto &thread: threads)
    {
      thread = nullptr;
    }
  Thread *main_thread = static_cast<Thread *>(malloc (sizeof (Thread)));
  if (main_thread == nullptr)
    {
      printf ("system error: failed to allocate memory. \n");
      exit (1);
    }

  main_thread->tid = 0;
  main_thread->state = RUNNING;
  main_thread->n_quantum = 1;
  main_thread->entry_point = nullptr;  // TODO??
  sigsetjmp(main_thread->env, 1);
//    address_t sp = (address_t) main_thread->stack + STACK_SIZE - sizeof
//        (address_t);
//    address_t pc = (address_t) main_thread->entry_point;
//    (main_thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
//    (main_thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
//    sigemptyset(&main_thread->env->__saved_mask);
  threads[0] = main_thread;
  current_thread = 0;

  // Set up the timer
  timer.it_value.tv_sec = quantum_usecs / 1000000;
  timer.it_value.tv_usec = quantum_usecs % 1000000;
  timer.it_interval.tv_sec = quantum_usecs / 1000000;
  timer.it_interval.tv_usec = quantum_usecs % 1000000;
  scheduler ();

  return 0;

}

void scheduler ()
{
  if (setitimer (ITIMER_VIRTUAL, &timer, nullptr) < 0)
    {
      printf ("system error: set timer failed.\n");
      exit (1);
    }

  // Check for threads to wake up
  for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
      if (threads[i] != nullptr && threads[i]->state == BLOCKED
          && threads[i]->wake_up_time <= uthread_get_total_quantums ())
        {
          threads[i]->state = READY;
          ready_queue.push (threads[i]);
        }
    }

  // Set up the signal handler
  struct sigaction sa = {0};
  sa.sa_handler = run_next_thread;
  if (sigaction (SIGVTALRM, &sa, nullptr) < 0)
    {
      printf ("system error: sigaction failed. \n");
      exit (1);
    }
  TOTAL_QUANTUMS++;
}

int uthread_spawn (thread_entry_point entry_point)
{
  if (entry_point == NULL)
    {
      printf ("thread library error: null entry point\n");
      return -1;
    }

  for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
      if (threads[i] == NULL)
        {
          Thread *new_thread = static_cast<Thread *>(malloc (sizeof (Thread)));
          if (new_thread == nullptr)
            {
              printf ("system error: memory allocation failed. \n");
              exit (1);
            }

          new_thread->tid = i;
          new_thread->state = READY;
          new_thread->n_quantum = 0;
          new_thread->entry_point = entry_point;
          sigsetjmp(new_thread->env, 1);
          address_t sp = (address_t) new_thread->stack + STACK_SIZE - sizeof
              (address_t);
          address_t pc = (address_t) entry_point;
          (new_thread->env->__jmpbuf)[JB_SP] = translate_address (sp);
          (new_thread->env->__jmpbuf)[JB_PC] = translate_address (pc);
          sigemptyset (&new_thread->env->__saved_mask);
          threads[i] = new_thread;
          ready_queue.push (new_thread);
          return i;
        }
    }
  return -1;
}

int uthread_terminate (int tid)
{
  if (!is_valid_tid (tid)) return -1;
  if (tid == 0)
    {
      exit (0);
    }

  if (threads[tid]->state == RUNNING)
    {
      scheduler ();
    }

  if (threads[tid]->state == READY)
    {
      remove_from_queue (tid);
    }
  free (threads[tid]);
  threads[tid] = nullptr;
  return 0;
}
void remove_from_queue (int tid)
{
  std::queue<Thread *> tempQueue;
  while (!ready_queue.empty ())
    {
      Thread *thread = ready_queue.front ();
      if (thread->tid != tid)
        {
          tempQueue.push (ready_queue.front ());
        }
      free (thread);
      ready_queue.pop ();
    }
  ready_queue = tempQueue;
}

int uthread_block (int tid)
{
  if (!is_valid_tid (tid)) return -1;

  if (tid == 0)
    {
      printf ("thread library error: blocking main thread.\n");
      return -1;
    }

  if (threads[tid]->state == RUNNING)
    {
      scheduler ();
    }

  if (threads[tid]->state == READY)
    {
      remove_from_queue (tid);
    }

  threads[tid]->state = BLOCKED;
  threads[tid]->wake_up_time = INFINITY;
  return 0;
}

int uthread_resume (int tid)
{
  if (!is_valid_tid (tid))
    {
      printf ("thread library error: Invalid input. \n");
      return -1;
    }

  threads[tid]->state = READY;
  ready_queue.push (threads[tid]);
  return 0;
}

int uthread_sleep (int num_quantums)
{
  if (current_thread == 0)
    {
      printf ("system error: main thread called sleep\n");
      exit (1);
    }

  Thread *thread = threads[current_thread];
  if (thread->state != RUNNING)
    {
      printf ("system error: thread is not running\n");
      exit (1);
    }

  thread->state = BLOCKED;
  thread->wake_up_time = uthread_get_total_quantums () + num_quantums;

  scheduler ();

  return 0;
}

int uthread_get_tid ()
{
  return current_thread;
}

int uthread_get_total_quantums ()
{
  return TOTAL_QUANTUMS;
}

int uthread_get_quantums (int tid)
{
  if (!is_valid_tid (tid)) return -1;

  return threads[tid]->n_quantum;
}
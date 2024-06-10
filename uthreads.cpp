#include "uthreads.h"
//#include <stdlib.h>
//#include <stdio.h>
#include <setjmp.h>
//#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>
#include "queue"
#include <cstdio>
#include <cerrno>
#include <iostream>

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define MILLION 1000000
#define FAILURE (-1)
#define SUCCESS 0
#define MAIN_THREAD 0

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7


int TOTAL_QUANTUMS = 0;

typedef struct Thread {
    int tid;
    int state;
    int n_quantum;
    float wake_up_time;
    char stack[STACK_SIZE];
    sigjmp_buf env;
} Thread;

//GQueue *ready_queue = NULL;
std::queue<Thread *> ready_queue;
Thread *threads[MAX_THREAD_NUM];
int current_thread = -1;
struct itimerval timer;
sigset_t masked;


void scheduler ();

void remove_from_queue (int tid);
void free_all_threads ();
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
      std::cerr <<  "thread library error: invalid tid\n";
      return false;
    }
  return true;
}

void select_and_run_next_thread ()
{
  Thread *next_thread = ready_queue.front ();
  ready_queue.pop ();
  current_thread = next_thread->tid;
  next_thread->state = RUNNING;
  next_thread->n_quantum++;
  // Restore the next thread's context
  siglongjmp (next_thread->env, 1);
}

// Signal handler for SIGVTALRM
void run_next_thread (int sig)
{
  // Save the current thread's context
  TOTAL_QUANTUMS++;
  // Check for threads to wake up
  for (auto &thread: threads)
    {
      if (thread != nullptr && thread->state == BLOCKED
          && thread->wake_up_time <= (float) uthread_get_total_quantums ())
        {
          thread->state = READY;
          ready_queue.push (thread);
        }
    }

  if (threads[current_thread])
    {
      if (sigsetjmp(threads[current_thread]->env, 1) == 0)
        {
          if (threads[current_thread]->state != BLOCKED)
            {
              ready_queue.push (threads[current_thread]);
              threads[current_thread]->state = READY;
            }
          select_and_run_next_thread ();
        }
    }
  else
    {
      select_and_run_next_thread ();
    }
}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      std::cerr << "thread library error: invalid quantum\n";
      return FAILURE;
    }

  for (auto &thread: threads)
    {
      thread = nullptr;
    }
  Thread *main_thread = static_cast<Thread *>(malloc (sizeof (Thread)));
  if (main_thread == nullptr)
    {
      std::cerr << "system error: failed to allocate memory. \n";
      return FAILURE;
    }

  main_thread->tid = MAIN_THREAD;
  main_thread->state = RUNNING;
  main_thread->n_quantum = 0;
  sigsetjmp(main_thread->env, 1);
  threads[MAIN_THREAD] = main_thread;
  current_thread = MAIN_THREAD;

  // Set up the timer
  timer.it_value.tv_sec = quantum_usecs / MILLION;
  timer.it_value.tv_usec = quantum_usecs % MILLION;
  timer.it_interval.tv_sec = quantum_usecs / MILLION;
  timer.it_interval.tv_usec = quantum_usecs % MILLION;

  // Create a sigset_t and add SIGVTALRM to it
  sigemptyset (&masked);
  sigaddset (&masked, SIGVTALRM);

  scheduler ();
  return SUCCESS;
}

void scheduler ()
{
  // Block signals
  sigprocmask (SIG_BLOCK, &masked, nullptr);
  if (setitimer (ITIMER_VIRTUAL, &timer, nullptr) < 0)
    {
      std::cerr << "system error: set timer failed.\n";
      return;
    }

  // Set up the signal handler
  struct sigaction sa = {0};
  sa.sa_handler = run_next_thread;
  if (sigaction (SIGVTALRM, &sa, nullptr) < 0)
    {
      std::cerr << "system error: sigaction failed. \n";
      return;
    }

  // Unblock signals
  sigprocmask (SIG_UNBLOCK, &masked, nullptr);
  run_next_thread (SIGALRM);
}

int uthread_spawn (thread_entry_point entry_point)
{
  // Block signals
  sigprocmask (SIG_BLOCK, &masked, nullptr);
  if (entry_point == nullptr)
    {
      std::cerr << "thread library error: null entry point\n";
      // Unblock signals before returning
      sigprocmask (SIG_UNBLOCK, &masked, nullptr);
      return FAILURE;
    }

  for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
      if (threads[i] == nullptr)
        {
          Thread *new_thread = static_cast<Thread *>(malloc (sizeof (Thread)));

          if (new_thread == nullptr)
            {
              std::cerr << "system error: memory allocation failed. \n";
              exit(1);
            }

          new_thread->tid = i;
          new_thread->state = READY;
          new_thread->n_quantum = 0;
          sigsetjmp(new_thread->env, 1);
          address_t sp = (address_t) new_thread->stack + STACK_SIZE - sizeof
              (address_t);
          address_t pc = (address_t) entry_point;
          (new_thread->env->__jmpbuf)[JB_SP] = translate_address (sp);
          (new_thread->env->__jmpbuf)[JB_PC] = translate_address (pc);
          sigemptyset (&new_thread->env->__saved_mask);
          threads[i] = new_thread;
          ready_queue.push (new_thread);

          // Unblock signals before returning
          sigprocmask (SIG_UNBLOCK, &masked, nullptr);
          return i;
        }
    }

    std::cerr << "thread library error: Too many threads. \n";
  // Unblock signals before returning
  sigprocmask (SIG_UNBLOCK, &masked, nullptr);
  return FAILURE;
}

int uthread_terminate (int tid)
{
  if (!is_valid_tid (tid)) return FAILURE;
  if (tid == MAIN_THREAD)
    {
      free_all_threads ();
      exit (SUCCESS);
    }

  if (threads[tid]->state == RUNNING)
    {
      free (threads[tid]);
      threads[tid] = nullptr;
      scheduler ();

      return SUCCESS;
    }

  if (threads[tid]->state == READY)
    {
      remove_from_queue (tid);
    }
  free (threads[tid]);
  threads[tid] = nullptr;
  return SUCCESS;
}

void free_all_threads ()
{
  for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
      if (threads[i] != nullptr)
        {
          free (threads[i]);
          threads[i] = nullptr;
        }
    }
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
      ready_queue.pop ();
    }
  ready_queue = tempQueue;
}

int uthread_block (int tid)
{
  if (!is_valid_tid (tid)) return FAILURE;

  if (tid == MAIN_THREAD)
    {
      std::cerr << "thread library error: blocking main thread.\n";
      return FAILURE;
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
  return SUCCESS;
}

int uthread_resume (int tid)
{
  if (!is_valid_tid (tid))
    {
      std::cerr << "thread library error: Invalid input. \n";
      return FAILURE;
    }

  threads[tid]->state = READY;
  ready_queue.push (threads[tid]);
  return SUCCESS;
}

int uthread_sleep (int num_quantums)
{
  if (current_thread == MAIN_THREAD)
    {
      std::cerr << "thread library error: main thread called sleep.\n";
      return FAILURE;
    }

  Thread *thread = threads[current_thread];
  if (thread->state != RUNNING)
    {
      std::cerr << "thread library error: thread is not running.\n";
      return FAILURE;
    }

  thread->state = BLOCKED;
  thread->wake_up_time = uthread_get_total_quantums () + num_quantums;

  scheduler ();
  return SUCCESS;
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
  if (!is_valid_tid (tid)) return FAILURE;

  return threads[tid]->n_quantum;
}
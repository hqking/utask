#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

/* implementation */
#include "../../src/task.c"

static int g_wake;

void taskLock(void)
{
}

void taskUnlock(void)
{
}

time_t taskGetTick(void)
{
  return time(NULL);
}

void taskIdleHook(time_t delay)
{
  time_t until;

  until = time(NULL) + delay;

  g_wake = 0;
  while(until > time(NULL)) {
    if (g_wake)
      break;
  }    
}

void taskWake(void)
{
  g_wake = 1;
}

/* testing file */

static FILE *g_log;

static void isr_ctx(int sig)
{
  /* read from pipe to know the actions to be taken */
}

static void loop_ctx(void)
{
  taskInit();
  taskSchedule();
}

static void outsidEvents(pid_t pid)
{
  /* fire event according to test case */
}

static void logCreate(void)
{
  char name[64];
  time_t now;

  now = time(NULL);

  memset(name, 0, sizeof(name));
  sprintf(name, "log_%d.cvs", now);

  g_log = fopen(name, "w");
}

int main(int argc, char *argv[])
{
  pid_t pid;

  signal(SIGINT, isr_ctx);

  /* open log files, in cvs format */
  logCreate();

  /* fork */
  pid = fork();
  if (pid == -1)
    return -1;

  if (pid == 0)
    loop_ctx();
  else
    outsidEvents(pid);

  fclose(g_log);

  return 0;
}

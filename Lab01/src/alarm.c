// Retransmission timer based on alarm() + sigaction

#include "alarm.h"

#include <stdio.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1

volatile sig_atomic_t alarmFired = FALSE;

static void alarmHandler(int signal)
{
    alarmFired = TRUE; 
}

int alarmSetup()
{
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

void alarmStart(int seconds)
{
    alarmFired = FALSE;
    alarm(seconds);
}

void alarmStop()
{
    alarm(0);
    alarmFired = FALSE;
}

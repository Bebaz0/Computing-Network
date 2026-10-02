// Retransmission timer based on alarm() + sigaction

#ifndef ALARM_H
#define ALARM_H

#include <signal.h>

// TRUE once the armed timer expires. Reset by alarmStart().
extern volatile sig_atomic_t alarmFired;

// Install the SIGALRM handler. Call once (e.g. in llOpen) before alarmStart().
// Returns 0 on success, -1 on error.
int alarmSetup();

// Arm the timer for `seconds` seconds and clear alarmFired.
void alarmStart(int seconds);

// Cancel the armed timer (e.g. when the expected response arrives).
void alarmStop();

#endif // ALARM_H

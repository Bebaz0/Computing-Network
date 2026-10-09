// Receiver state machine for supervision frames (SET, UA, ...)

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#define FLAG 0x7E

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
} State;

typedef struct
{
    State state;
    unsigned char expectedA;
    unsigned char expectedC;
} StateMachine;

void smInit(StateMachine *sm, unsigned char A, unsigned char C);


int smProcessByte(StateMachine *sm, unsigned char byte);

#endif

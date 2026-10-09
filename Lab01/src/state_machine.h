// Receiver state machine for supervision frames (SET, UA, ...)

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#define FLAG 0x7E
#define SM_ANY_C -1 // aceita qualquer C (guardado em sm->c)

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
    int expectedC;   // C esperado, ou SM_ANY_C
    unsigned char c; // C recebido
} StateMachine;

void smInit(StateMachine *sm, unsigned char A, int C);


int smProcessByte(StateMachine *sm, unsigned char byte);

#endif

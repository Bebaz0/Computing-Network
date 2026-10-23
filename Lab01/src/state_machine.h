// Receiver state machine for supervision (SET, UA, ...) and information frames

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "link_layer.h"

#define FLAG 0x7E
#define ESC 0x7D
#define SM_ANY_C -1 // aceita qualquer C (guardado em sm->c)

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_RCV,
    STOP
} State;

typedef struct
{
    State state;
    unsigned char expectedA;
    int expectedC;   // C esperado, ou SM_ANY_C
    unsigned char c; // C recebido
    unsigned char data[MAX_PAYLOAD_SIZE + 1]; // I frame: dados destuffed, ultimo byte = BCC2
    int dataLen;
    int escaped;     // ultimo byte foi ESC
} StateMachine;

void smInit(StateMachine *sm, unsigned char A, int C);


int smProcessByte(StateMachine *sm, unsigned char byte);

#endif

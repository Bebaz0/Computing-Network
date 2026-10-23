// Receiver state machine for supervision (SET, UA, ...) and information frames

#include "state_machine.h"

void smInit(StateMachine *sm, unsigned char A, int C)
{
   sm->state= START;
   sm->expectedA = A;
   sm->expectedC = C;
   sm->dataLen = 0;
   sm->escaped = 0;
}

int smProcessByte(StateMachine *sm, unsigned char byte)
{
    switch (sm->state)
    {
    case START:
        if (byte == FLAG)
        {
            sm->state = FLAG_RCV;
        }
        
        
        break;

    case FLAG_RCV:
        if (byte == sm->expectedA)
        {
            sm->state = A_RCV;
        }
        else if (byte != FLAG)
        {
            sm->state = START;
        }
        

        break;

    case A_RCV:
        if (byte == FLAG)
        {
            sm->state = FLAG_RCV;
        }
        else if (sm->expectedC == SM_ANY_C || byte == sm->expectedC)
        {
            sm->c = byte;
            sm->state = C_RCV;
        }
        else
        {
            sm->state = START;
        }
        break;

    case C_RCV:
        if (byte == (sm->expectedA ^ sm->c))
        {
            sm->state = BCC_OK;
        }
        else if (byte != FLAG)
        {
            sm->state = START;
        }
        else{
            sm->state = FLAG_RCV;
        }
        break;

    case BCC_OK:
        sm->dataLen = 0;
        sm->escaped = 0;
        if (byte == FLAG)
        {
            sm->state = STOP; // trama S/U
            break;
        }
        if (sm->c != 0x00 && sm->c != 0x80) // so tramas I tem dados
        {
            sm->state = START;
            break;
        }
        sm->state = DATA_RCV;
        // fall through: este byte ja e o primeiro dado

    case DATA_RCV:
        if (byte == FLAG)
        {
            sm->state = STOP;
        }
        else if (byte == ESC)
        {
            sm->escaped = 1;
        }
        else if (sm->dataLen == sizeof(sm->data))
        {
            sm->state = START; // trama demasiado grande, descartar
        }
        else
        {
            sm->data[sm->dataLen++] = sm->escaped ? byte ^ 0x20 : byte; // destuffing
            sm->escaped = 0;
        }
        break;

    case STOP:
        return 1;
    }

    return sm->state == STOP;
}

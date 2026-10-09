// Receiver state machine for supervision frames (SET, UA, ...)

#include "state_machine.h"

void smInit(StateMachine *sm, unsigned char A, int C)
{
   sm->state= START;
   sm->expectedA = A;
   sm->expectedC = C;
   
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
        if (byte == FLAG)
        {
            sm->state = STOP;
        }
        else{
            sm->state = START;
        }
        
        break;

    case STOP:
        return 1;
    }

    return sm->state == STOP;
}

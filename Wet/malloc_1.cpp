#include <unistd.h>
#define MAX_SIZE (1e8)

// DONE
void *smalloc(size_t size)
{
    if ( (size == 0) || (size > MAX_SIZE) )
    {    
            // invalid size
            return NULL;
    }
    void *program_break = sbrk(size);
    if ( program_break == (void*)(-1) )
    {
        // sbrk() failed
        return NULL;
    }
    return program_break;
}

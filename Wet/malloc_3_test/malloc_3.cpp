#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define MAX_SIZE (1e8)
#define MIN_SPLIT_THRESHOLD (128)


class MallocMedata
{
public:
    size_t size;
    bool is_free;
    void* addr;
    MallocMedata* next;
    MallocMedata* prev;
    MallocMedata(size_t size = 0, bool is_free = false, MallocMedata* next = NULL, MallocMedata* prev = NULL);
    ~MallocMedata() = default;
    bool operator==(MallocMedata& other);
    bool operator< (MallocMedata& other);
    bool operator>=(MallocMedata& other);
    bool operator> (MallocMedata& other);
    bool operator<=(MallocMedata& other);
};

MallocMedata::MallocMedata(size_t size, bool is_free, MallocMedata* next, MallocMedata* prev):
    size(size),
    is_free(is_free),
    next(next),
    prev(prev)
{}
bool MallocMedata::operator==(MallocMedata& other)
{
    return (this->size == other.size && this->addr == other.addr);
}
bool MallocMedata::operator<(MallocMedata& other)
{
    if ( this->size < other.size )
    {
        return true;
    }
    else if (this->size == other.size)
    {
        return (this->addr < other.addr);
    }
    //else
    return false;
}
bool MallocMedata::operator>=(MallocMedata& other)
{
    return !((*this) < other);
}

bool MallocMedata::operator>(MallocMedata& other)
{
    return ( (*this >= other) && !(*this == other) );    
}

bool MallocMedata::operator<=(MallocMedata& other)
{
    return( !((*this) > other) );
}

static MallocMedata head_data = MallocMedata();

class FreeList
{
private:
    // MallocMedata *head;
    // MallocMedata *tail;
public:
    MallocMedata *head;
    MallocMedata *wild_chunk;
    long long int num_allocated_bytes;
    long long int num_free_bytes;
    long long int num_allocated_blocks;
    long long int num_free_blocks;
    FreeList();
    ~FreeList() = default;
    MallocMedata* SearchBlock(size_t size);
    MallocMedata* SearchBlockMeData(MallocMedata *data);
    void AddMeData_Block(MallocMedata* data, size_t size, void* addr);
    MallocMedata* RemoveMeData_Block(MallocMedata* data);
    void SortList();
    void Swap(MallocMedata* A, MallocMedata* B);
    MallocMedata *SearchWildBlock();
};

FreeList::FreeList()
{
    this->head = &head_data;
    this->wild_chunk = NULL;
    this->num_free_blocks = 0;
    this->num_allocated_blocks = 0;
    this->num_free_bytes = 0;
    this->num_allocated_bytes = 0;
}

MallocMedata* FreeList::SearchBlock(size_t size)
{
    for ( MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        if (curr->size >= size && curr->is_free == true)
        {
            return curr;
        } 
    }
    return NULL;
}

MallocMedata* FreeList::SearchBlockMeData(MallocMedata *data)
{
    for ( MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        if ( (*curr) == (*data) )
        {
            return curr;
        } 
    }
    return NULL;
}

MallocMedata *FreeList::SearchWildBlock()
{
    MallocMedata *wild =NULL;
    for ( MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        if (wild == NULL)
        {
            wild = curr;
        }
        else
        {
            wild = (curr->addr > wild->addr) ? curr : wild;
        }
    }
    return wild;
}

/* Add new block to the list and sort the list */
void FreeList::AddMeData_Block(MallocMedata* data, size_t size, void* addr)
{
    data->size = size;
    data->is_free = false;
    data->addr = addr;

    for ( MallocMedata *curr = this->head; curr != NULL; curr = curr->next)
    {
        if (curr->next == NULL)
        {
            curr->next = data;
            data->prev = curr;
            data->next = NULL;
            break;
        } 
    }

    SortList();
}
MallocMedata* FreeList::RemoveMeData_Block(MallocMedata* data)
{
    // to check this!
    for ( MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        if (curr == data)
        {
            if (curr->next == NULL)
            {
                (curr->prev)->next = NULL;
                return curr;
            }
            else
            {
                (curr->prev)->next = curr->next;
                (curr->next)->prev = curr->prev;
                return curr;
            }   
        }
    }   
    return NULL;
}

void FreeList::SortList()
{
    size_t list_size = 0;
    for ( MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        list_size++; 
    }
    
    if(list_size <= 1)
    {
        return; // list is empty
    }
    
    for (MallocMedata *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        for (MallocMedata *curr_inner = curr->next; curr_inner != NULL; curr_inner = curr_inner->next )
        {
            if ( (*curr_inner) < (*curr) )
            {
                Swap(curr_inner ,curr );
                
                MallocMedata* tmp = curr_inner;
                curr_inner = curr;
                curr = tmp;
                
            }
        }
        
    }
    

}

/* Input : Head <--> A <--> C <--> D <--> B <--> NULL */
/* Output: Head <--> B <--> C <--> D <--> A <--> NULL */
void FreeList::Swap(MallocMedata* A, MallocMedata* B)
{
    if(!A || !B) //sanity check
        return;
    
    if(*A < *B)
    {
        MallocMedata* tmp = A;
        A = B;
        B = tmp;
    }

    MallocMedata* prevA = A->prev;
    MallocMedata* nextA = A->next;
    MallocMedata* prevB = B->prev;
    MallocMedata* nextB = B->next;
    
    // A = 95
    // B = 100
    // if (nextA == prevB)
    // {
    //     /* code */
    // }
    
    if(prevA != NULL)
    {
        prevA->next = B;
    }
    if(nextA != NULL && nextA != B)
    {
        nextA->prev = B;
    }

    
    if(nextB != NULL)
    {
        nextB->prev = A;
    }
    if(prevB != NULL && prevB != A)
    {
        prevB->next = A;
    }

    B->prev = prevA;
    if(nextA != B)
        B->next = nextA;
    else
        B->next = A;

    A->next = nextB;
    if(prevB != A)
        A->prev = prevB;
    else
        A->prev = B;

}

static FreeList free_list = FreeList();

void *smalloc(size_t size)
{
    if (size <= 0 || size > MAX_SIZE)
    {    
        return NULL; // invalid size
    }

    // valid size:
    MallocMedata* found = free_list.SearchBlock(size); //return the first MeData block with atleast size bytes
    if(found == NULL)
    {
        // wilderness chunk implementation:
        // to test later
        MallocMedata* found_wild = free_list.SearchWildBlock();
        if ( found_wild && found_wild->is_free ) // found is the last block in the list
        {
            size_t size_diff = size - found_wild->size;
            void *wildp = sbrk(size_diff);
            found_wild->size += size_diff;
            found_wild->is_free = false;
            free_list.num_allocated_bytes += size_diff;
            if( wildp == (void*)(-1))
            {
                return NULL;
            }
            return found_wild->addr;
        }
        void *oldp = sbrk(0);
        void* p = sbrk(size + sizeof(MallocMedata));
        free_list.num_allocated_blocks++; // allocated a new block
        free_list.num_allocated_bytes += size; // sizeof the new block
        if(p == (void*)(-1))
        {
            return NULL;
        }

        free_list.AddMeData_Block( static_cast<MallocMedata*>(oldp) , size, (void*)( (char*)oldp + sizeof(MallocMedata)) ); //Add the new block to end of the list.

        return (void*)( (char*)(oldp) + sizeof(MallocMedata));
    }
    else
    {
        if (found->size > size + MIN_SPLIT_THRESHOLD + sizeof(MallocMedata) )
        {
            MallocMedata *split_data = (MallocMedata*)((char*)found + sizeof(MallocMedata) + size);
            split_data->size = found->size - (size + sizeof(MallocMedata) );
            // split_data->is_free = true; //this has to happen after addmedata_block()
            split_data->addr = (char*)split_data + sizeof(MallocMedata);
            
            found->size = size;
            found->is_free = false;
            
            free_list.AddMeData_Block(split_data,split_data->size,split_data->addr); // calls sortlist()
            split_data->is_free = true;

            free_list.num_allocated_blocks++;
            free_list.num_allocated_bytes += found->size;
            free_list.num_free_bytes -= found->size; // the new found->size value
            return found->addr;
        }        
        
        found->is_free = false;
        free_list.num_allocated_blocks++;
        free_list.num_allocated_bytes += found->size;
        free_list.num_free_blocks--;
        free_list.num_free_bytes -= found->size;
        
        return (void*) ( (char*)(found) + sizeof(MallocMedata));
    }
    
}

void *scalloc(size_t num, size_t size)
{
    if (num <= 0 || size <= 0 || (size*num) > MAX_SIZE)
    {
        return NULL; // invalid arguments
    }

    // valid arguments:
    void* p = smalloc(num*size);
    if(p == NULL)
        return NULL;

    //void * memset ( void * ptr, int value, size_t num );
    memset(p, 0, num*size);
    
    return p;
}

void sfree(void* p)
{
    if (p == NULL)
    {
        return;
    }
    MallocMedata *datap = (MallocMedata*)((char*)p - sizeof(MallocMedata));
    MallocMedata *data_prev = datap->prev;
    MallocMedata *data_next = datap->next;
    bool was_prev_free = data_prev->is_free && (data_prev != free_list.head);
    if(datap->is_free == true)
    {
        return;
    }
    else
    {
        bool unified_blocks = false;
        if ( (data_prev->is_free == true) && (data_prev != free_list.head))
        {
            // 1) adjust size of data_prev to unified size
            data_prev->size += datap->size + sizeof(MallocMedata);// sizeof(...) to account for the header
            // 2) remove datap from the free_list
            MallocMedata *removed_data = free_list.RemoveMeData_Block(datap);

            free_list.num_allocated_blocks--;
            free_list.num_allocated_bytes -= datap->size; 
            free_list.num_free_bytes += datap->size;

            unified_blocks = true;
        }
        if ( data_next != NULL && data_next->is_free == true)
        {
            MallocMedata *removed_next_data = NULL;
            if (was_prev_free)
            {
                // 1) adjust size of data_prev to unified size
                data_prev->size += data_next->size + sizeof(MallocMedata);// sizeof(...) to account for the header
                // 2) remove data_next from the free_list
                removed_next_data = free_list.RemoveMeData_Block(data_next);

                free_list.num_allocated_blocks--;
                free_list.num_free_blocks--;
                free_list.num_allocated_bytes -= datap->size; 
                free_list.num_free_bytes += datap->size;
                
                unified_blocks = true;
            }
            else
            {
                // 1) adjust size of datap to unified size
                datap->size += data_next->size + sizeof(MallocMedata);// sizeof(...) to account for the header
                // 2) remove data_NEXT from the free_list
                removed_next_data = free_list.RemoveMeData_Block(data_next);

                datap->is_free = true;
                free_list.num_allocated_blocks--;
                free_list.num_allocated_bytes -= datap->size; 
                free_list.num_free_bytes += datap->size;

                unified_blocks = true;
            }            
        }
        
        if (unified_blocks)
        {
           return;
        }
        

        free_list.num_allocated_blocks--;
        free_list.num_free_blocks++;
        free_list.num_allocated_bytes -= datap->size; // sizeof the block to free
        free_list.num_free_bytes += datap->size;
        datap->is_free = true;
    }
}


void* srealloc(void* oldp, size_t size)
{
    if (size <= 0 || size > MAX_SIZE)
    {
        return NULL;
    }

    if(oldp == NULL)
    {
        return smalloc(size);
    }
    
    MallocMedata *datap = (MallocMedata*)((char*)oldp - sizeof(MallocMedata));
    if(size <= datap->size)
    {
        return oldp;
    }
    
    //1. Find / allocate new block of 'size' bytes.
    void* newp = smalloc(size);
    if(newp == NULL)
    {
        return NULL;
    }
    
    //2. copy oldp DATA to the new block DATA
    //void * memmove ( void * destination, const void * source, size_t num );
    if(memmove(newp, oldp, datap->size) != newp) //memmove returns the destination pointer.
    {
        return NULL;
    }

    //3. free oldp:
    sfree(oldp);
    return newp;

}

 size_t _num_free_blocks()
 {
    return free_list.num_free_blocks;
 }

 size_t _num_free_bytes()
 {
    return free_list.num_free_bytes;
 }

 size_t _num_allocated_blocks()
 {
    // return free_list.num_allocated_blocks;
    return free_list.num_allocated_blocks + free_list.num_free_blocks; // look at function 7
 }
 
 size_t _num_allocated_bytes()
 {
    // return free_list.num_allocated_bytes;
    return free_list.num_allocated_bytes + free_list.num_free_bytes;
 }

size_t _size_meta_data()
{
    return (sizeof(MallocMedata));
}

size_t _num_meta_data_bytes()
{
    return (_size_meta_data() * _num_allocated_blocks());
}

void DEBUG_PrintList()
{
    MallocMedata *p = head_data.next;
    printf("\nPrinting list:\n(size, addr)\n");
    printf("HEAD <--> ");
    for (; p != NULL; p = p->next)
    {
        if(!p->is_free)
            printf("(%d, %p) <--> ", p->size, p->addr);
    }
    printf("NULL\n");

    
    
    
    
    p = head_data.next;
    printf("HEAD <--> ");
    for (; p != NULL; p = p->next)
    {
        printf("(%d, %p) <--> ", p->size, p->addr);
    }
    printf("NULL\n");
}
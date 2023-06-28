#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <cmath> // necessary for calculations

#define MAX_SIZE (1e8)
#define KB (1024)
#define MY_MMAP_THRESHOLD (128*KB)
#define DEFAULT_BUDDY_BLOCK (128*KB)
#define MIN_BUDDY_BLOCK (128)
#define BUDDY_BLOCKS_NUM 32
#define MAX_ORDER 10

class MallocMetaData
{
public:
    int cookies; // it's essential that the cookies are placed at the beginning of the block.
    size_t size;
    bool is_free;
    void* addr;
    MallocMetaData* next;
    MallocMetaData* prev;
    MallocMetaData(int cookies = 0, size_t size = 0, bool is_free = false, MallocMetaData* next = NULL, MallocMetaData* prev = NULL);
    ~MallocMetaData() = default;
    bool operator==(MallocMetaData& other);
    bool operator< (MallocMetaData& other);
    bool operator>=(MallocMetaData& other);
    bool operator> (MallocMetaData& other);
    bool operator<=(MallocMetaData& other);
};

MallocMetaData::MallocMetaData(int cookies, size_t size, bool is_free, MallocMetaData* next, MallocMetaData* prev):
    cookies(cookies),
    size(size),
    is_free(is_free),
    next(next),
    prev(prev)
{}

bool MallocMetaData::operator==(MallocMetaData& other)
{
    return (this->size == other.size && this->addr == other.addr);
}

bool MallocMetaData::operator<(MallocMetaData& other)
{
    if ( this->size < other.size )
    {
        return true;
    }
    else if (this->size == other.size)
    {
        return (this->addr < other.addr);
    }
    return false;
}

bool MallocMetaData::operator>=(MallocMetaData& other)
{
    return !((*this) < other);
}

bool MallocMetaData::operator>(MallocMetaData& other)
{
    return ( (*this >= other) && !(*this == other) );    
}

bool MallocMetaData::operator<=(MallocMetaData& other)
{
    return( !((*this) > other) );
}

static void Swap(MallocMetaData* A, MallocMetaData* B)
{
    if(!A || !B) //sanity check
        return;
    
    if(*A < *B)
    {
        MallocMetaData* tmp = A;
        A = B;
        B = tmp;
    }

    MallocMetaData* prevA = A->prev;
    MallocMetaData* nextA = A->next;
    MallocMetaData* prevB = B->prev;
    MallocMetaData* nextB = B->next;
    
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

static void initializeData(MallocMetaData* head_datas, MallocMetaData* tail_datas)
{
    for (int i = 0; i < (BUDDY_BLOCKS_NUM+1); i++)
    {
        head_datas[i] = MallocMetaData();
        tail_datas[i] = MallocMetaData();
    }
}

static int getOrderFromSize(size_t size)
{
    for (int i = 0; i < (MAX_ORDER); i++)
    {
        double power = pow(2,i);
        if ( static_cast<int>(power) == size)
        {
            return i;
        }
    }
    return -1;
}

class MyList
{
public:
    MallocMetaData* head;
    MallocMetaData* tail;
    MyList();
    MyList(MallocMetaData* head_node, MallocMetaData* tail_node);
    ~MyList() = default;
    void insert(MallocMetaData* node);
    void SortMallocMetaDataList(MallocMetaData* head_node, MallocMetaData* tail_node);
    MallocMetaData* findBlock(MallocMetaData block);
    int removeBlock(MallocMetaData* block);
};

MyList::MyList()
{
    this->head = NULL;
    this->tail = NULL;
}

MyList::MyList(MallocMetaData* head_node, MallocMetaData* tail_node)
{
    this->head = head_node;
    this->tail = tail_node;
}

void MyList::insert(MallocMetaData* node)
{
    this->tail->prev->next = node;
    this->tail->prev = node;
    SortMallocMetaDataList(this->head, this->tail);
}

void MyList::SortMallocMetaDataList(MallocMetaData* head_node, MallocMetaData* tail_node)
{
    size_t list_size = 0;
    for ( MallocMetaData *curr = this->head->next; curr != tail_node; curr = curr->next)
    {
        list_size++; 
    }
    
    if(list_size <= 1)
    {
        return;
    }
    
    for (MallocMetaData *curr = this->head->next; curr != tail_node; curr = curr->next)
    {
        for (MallocMetaData *curr_inner = curr->next; curr_inner != tail_node; curr_inner = curr_inner->next )
        {
            if ( (*curr_inner) < (*curr) )
            {
                Swap(curr_inner ,curr ); 
                MallocMetaData* tmp = curr_inner;
                curr_inner = curr;
                curr = tmp;  
            }
        }   
    }
}

MallocMetaData* MyList::findBlock(MallocMetaData block)
{
    for (MallocMetaData *curr = this->head->next; curr != this->tail; curr = curr->next )
    {
        if ( (*curr) == block )
        {
            return curr;
        }
    } 
    return NULL;  
}

int MyList::removeBlock(MallocMetaData* block)
{
    MallocMetaData* found = findBlock(*block);
    if (!found)
    {
        return -1;
    }
    else
    {
        MallocMetaData* tmp = block->prev;
        block->prev->next = block->next;
        block->next->prev = tmp;
    }
    return 1;
}

class FreeList
{
public:
    int cookies;
    MallocMetaData* head;
    MallocMetaData* tail;
    void* wasted_block;
    size_t wasted_block_size;
    MyList orders_list[ MAX_ORDER + 1];

    size_t num_allocated_bytes;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_free_blocks;

    FreeList(MallocMetaData* head_node, MallocMetaData* tail_node, MallocMetaData* head_datas, MallocMetaData* tail_datas);
    ~FreeList() = default;
    void initializeBuddySystem(MallocMetaData* head_nodes, MallocMetaData* tail_nodes);
    void SortMallocMetaDataList(MallocMetaData* head_node, MallocMetaData* tail_node);
    MallocMetaData* findBlock(size_t required_size);
    MallocMetaData* splitBlock(MallocMetaData* data, size_t min_size);
    MallocMetaData* mergeBlocks(MallocMetaData* prev, MallocMetaData* curr);
    MallocMetaData* findNextBuddy(MallocMetaData* block);
    MallocMetaData* findPreviousBuddy(MallocMetaData* block);
    void insertToFreeList(MallocMetaData* block);
    void deleteFromFreeList(MallocMetaData* block);
    void* addMapping(size_t size);
    void* allocateBlock(size_t size);
};

void FreeList::initializeBuddySystem(MallocMetaData* head_nodes, MallocMetaData* tail_nodes)
{
    // allign allocations to start at a multiple of (32*128*KB) to ease calculations later
    void* curr_prog_break = sbrk(0);
    size_t align_to_num = (32*128*KB);
    size_t alloc_alignment_size = ( (size_t)((char*)curr_prog_break) % align_to_num ); // check correctness!!!!!!!!!!!
    this->wasted_block = curr_prog_break;
    this->wasted_block_size = alloc_alignment_size;


    // allocate the new aray of size (32*128*KB)
    void* new_list = sbrk(32*128*KB);
    for (int i = 0; i < BUDDY_BLOCKS_NUM; i++)
    {
        MallocMetaData* curr = (MallocMetaData*)((char*)new_list + (i*DEFAULT_BUDDY_BLOCK));
        MallocMetaData* next = (MallocMetaData*)((char*)new_list + ((i+1)*DEFAULT_BUDDY_BLOCK));
        MallocMetaData* prev = (MallocMetaData*)((char*)new_list + ((i-1)*DEFAULT_BUDDY_BLOCK));
        curr->size = (128*KB) - sizeof(MallocMetaData);
        curr->is_free = true;
        curr->cookies = this->cookies;
        this->orders_list[MAX_ORDER].insert(curr); // at the beginning they're all inserted with size = 128KB
        this->num_free_bytes += curr->size;
        this->num_free_blocks += 1;
    }
    this->num_allocated_blocks = 0;
    this->num_allocated_bytes = 0;
}

FreeList::FreeList(MallocMetaData* head_node, MallocMetaData* tail_node, MallocMetaData* head_datas, MallocMetaData* tail_datas)
{
    srand(0);
    this->cookies = rand();
    this->head = head_node;
    this->tail = tail_node;
    this->head->cookies = this->cookies;
    initializeData(head_datas, tail_datas);
    for (int i = 0; i < (MAX_ORDER+1); i++)
    {
        this->orders_list[i] = MyList( &head_datas[i], &tail_datas[i]); 
    }

    this->num_free_blocks = 0;
    this->num_allocated_blocks = 0;
    this->num_free_bytes = 0;
    this->num_allocated_bytes = 0;
}

void FreeList::SortMallocMetaDataList(MallocMetaData* head_node, MallocMetaData* tail_node)
{
    size_t list_size = 0;
    for ( MallocMetaData *curr = this->head->next; curr != tail_node; curr = curr->next)
    {
        list_size++; 
    }
    
    if(list_size <= 1)
    {
        return;
    }
    
    for (MallocMetaData *curr = this->head->next; curr != tail_node; curr = curr->next)
    {
        for (MallocMetaData *curr_inner = curr->next; curr_inner != tail_node; curr_inner = curr_inner->next )
        {
            if ( (*curr_inner) < (*curr) )
            {
                Swap(curr_inner ,curr ); 
                MallocMetaData* tmp = curr_inner;
                curr_inner = curr;
                curr = tmp;  
            }
        }   
    }
}

MallocMetaData* FreeList::findBlock(size_t required_size)
{
    for (int i = 0; i < (MAX_ORDER+1); i++)
    {
        for (MallocMetaData* curr = this->orders_list[i].head; curr != this->orders_list[i].tail; i++)
        {
            if (curr->size >= required_size && curr->is_free == true)
            { // since the list is sorted we'll find the smallest size first.
                return curr;
            }     
        }
    }
    return NULL;
}

/*
potential errors
Useful edge case tests:
1) when should we stop splitting? when block->size <= 2*min_size or when (block->size + sizeof(MallocMetaData)) <= 2*min_size
*/
MallocMetaData* FreeList::splitBlock(MallocMetaData* data, size_t min_size)
{
    // min_size is the size of the block we want to insert!
    MallocMetaData* new_block = NULL;
    MallocMetaData* curr_block = data;
    int current_size = curr_block->size;
    int new_size = (current_size - sizeof(MallocMetaData))/2; // the size of the block after splitting
    while ( min_size <= new_size )
    {
        current_size = curr_block->size;
        new_size = (current_size - sizeof(MallocMetaData))/2;
        if ( (current_size + sizeof(MallocMetaData)) > MIN_BUDDY_BLOCK)
        {
            curr_block->size = new_size;
            new_block = (MallocMetaData*)((char*)curr_block + new_size );
            new_block->cookies = curr_block->cookies;
            new_block->size = new_size;
            new_block->is_free = true;
            new_block->addr = (void*)((char*)new_block + sizeof(MallocMetaData));
            int index_old = getOrderFromSize( current_size + sizeof(MallocMetaData) );
            int index_new = getOrderFromSize( new_size + sizeof(MallocMetaData) );
            this->orders_list[index_old].removeBlock(curr_block);
            this->orders_list[index_new].insert(curr_block);
            this->orders_list[index_new].insert(new_block);
            current_size = new_size;
            this->num_free_blocks += 1;
            this->num_free_bytes -= sizeof(MallocMetaData);
        }
        else
        {
            // can't split a block with the minimum size.
            return NULL;
        }
    }
    return curr_block;
}

MallocMetaData* FreeList::mergeBlocks(MallocMetaData* prev, MallocMetaData* curr)
{
    int index_prev = getOrderFromSize(prev->size + sizeof(MallocMetaData));
    int index_curr = getOrderFromSize(curr->size + sizeof(MallocMetaData));
    this->orders_list[index_prev].removeBlock(prev);
    this->orders_list[index_curr].removeBlock(curr);
    prev->size += curr->size + sizeof(MallocMetaData);
    int new_index = getOrderFromSize( prev->size + sizeof(MallocMetaData));
    this->orders_list[new_index].insert(prev);
    this->num_free_blocks -= 1;
    this->num_free_bytes += sizeof(MallocMetaData);
    return prev;
}

MallocMetaData* FreeList::findNextBuddy(MallocMetaData* block)
{
    for (int i = 0; i < (MAX_ORDER + 1); i++)
    {
        for (MallocMetaData* curr = this->orders_list[i].head->next; curr != this->orders_list[i].tail; curr = curr->next)
        {
            if ( ( (char*)((char*)block + block->size + sizeof(MallocMetaData)) == (char*)(curr) ) && curr->is_free )
            {
                return curr;
            }  
        }
    }
    return NULL;
}

MallocMetaData* FreeList::findPreviousBuddy(MallocMetaData* block)
{
    for (int i = 0; i < (MAX_ORDER + 1); i++)
    {
        for (MallocMetaData* curr = this->orders_list[i].head->next; curr != this->orders_list[i].tail; curr = curr->next)
        {
            if ( ( (char*)((char*)curr + curr->size + sizeof(MallocMetaData)) == (char*)(block) ) && curr->is_free )
            {
                return curr;
            }  
        }
    }
    return NULL;
}

void FreeList::insertToFreeList(MallocMetaData* block)
{
    this->head->next->prev = block;
    this->head->next = block;
    SortMallocMetaDataList(this->head, this->tail);
}

void FreeList::deleteFromFreeList(MallocMetaData* block)
{
    for ( MallocMetaData* curr = this->head->next; curr != this->tail; curr = curr->next)
    {
        if ( curr == block)
        {
            (curr->prev)->next = curr->next;
            (curr->next)->prev = curr->prev;
        }
        
    }
    
}

void* FreeList::addMapping(size_t size)
{
    void* allocation = mmap(NULL, (size + sizeof(MallocMetaData)), (PROT_EXEC | PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0); // correct flags / prot?
    if (allocation == MAP_FAILED)
    {
        return NULL;
    }
    
    MallocMetaData* data = (MallocMetaData*)allocation;
    data->addr = (void*)((char*)allocation + sizeof(MallocMetaData));
    data->is_free = false;
    data->size = size;
    data->cookies = this->cookies;
    this->insertToFreeList(data);

    this->num_allocated_bytes += (size + sizeof(MallocMetaData));
    this->num_allocated_blocks += 1;
    
    return data->addr;
}

void* FreeList::allocateBlock(size_t size)
{
    MallocMetaData* found = this->findBlock(size);
    if (!found)
    {
        // shouldn't allocate more, try looking for a block that's empty and merge it with its buddy.
        return NULL;
    }
    else
    {
        if (found->cookies != this->cookies)
        {
            // there was an overflow that corrupted our data.
            exit(0xdeadbeef);
        }

        if ( (found->size - sizeof(MallocMetaData)) > 2*size )
        {
            MallocMetaData* split_block = this->splitBlock(found, size);
            /* potential errors*/
            // things to consider: when splitting the blocks I should change the number of free_bytes, block...
            split_block->is_free = false;
            return split_block->addr;
        }
        else
        {
            found->is_free = false;
            return found->addr;
        }
    }
    return NULL;
}

// the following elements are allocated on the stack!
static MallocMetaData head_datas[BUDDY_BLOCKS_NUM+1];
static MallocMetaData tail_datas[BUDDY_BLOCKS_NUM+1];
static MallocMetaData head_node = MallocMetaData();
static MallocMetaData mmap_head_node = MallocMetaData();
static MallocMetaData tail_node = MallocMetaData();
static MallocMetaData mmap_tail_node = MallocMetaData();
static FreeList free_list = FreeList(&head_node, &tail_node, head_datas, tail_datas);
static FreeList mmap_free_list = FreeList(&mmap_head_node, &mmap_tail_node, head_datas, tail_datas);
static bool buddy_system_init = false;

void *smalloc(size_t size)
{
    if (!buddy_system_init)
    {
        free_list.initializeBuddySystem(head_datas, tail_datas);
        buddy_system_init = true;
    }
    
    if (size <= 0)
    {    
        /* 
        potential error:
            consider checking if size > MAX_SIZE. 
            is it a case for mmap or is it an error?
        */
        return NULL;
    }
    if (size >= MY_MMAP_THRESHOLD)
    {
        /*
        potential errors:
            should we check if (datap->size > MY_MMAP_THRESHOLD) or (datap->size >= MY_MMAP_THRESHOLD)?
        */
        return mmap_free_list.addMapping(size);
    }
    else
    {
        return free_list.allocateBlock(size);
    }
    return NULL;
}

void *scalloc(size_t num, size_t size)
{
    if (num <= 0 || size <= 0 || (size*num) > MAX_SIZE)
    {
        /* 
        potential error:
            consider not checking if num*size > MAX_SIZE. 
            is it a case for mmap or is it an error?
        */
        return NULL;
    }
    void* allocation = smalloc(num*size);
    if(allocation == NULL)
    {
        return NULL;
    }
    memset(allocation, 0, num*size);
    return allocation;
}

void sfree(void* p)
{
    if (p == NULL)
    {
        return;
    }
    MallocMetaData *datap = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));
    if (datap->size >= MY_MMAP_THRESHOLD)
    {
        /*
        potential errors:
            should we check if (datap->size > MY_MMAP_THRESHOLD) or (datap->size >= MY_MMAP_THRESHOLD)?
            according to the notes in section 3, we shouldn't consider munmapped areas as freed
        */
        munmap(datap, datap->size + sizeof(MallocMetaData));
    }
    else
    {
        MallocMetaData *datap_prev = free_list.findPreviousBuddy(datap);
        MallocMetaData *datap_next = free_list.findNextBuddy(datap);

        if (datap->cookies != free_list.cookies ||
            (datap_prev != NULL && datap_prev->cookies != free_list.cookies) ||
            (datap_next != NULL && datap_next->cookies != free_list.cookies))
        {
            // an overflow occured and someone used our data.
            exit(0xdeadbeef);
        }
        MallocMetaData* merged = NULL;
        if (datap_prev != NULL)
        {
            merged = free_list.mergeBlocks(datap_prev, datap);
            if (datap_next != NULL)
            {
                merged = free_list.mergeBlocks(merged, datap_next);
            }
        }
        else
        {
            if (datap_next != NULL)
            {
                merged = free_list.mergeBlocks(datap, datap_next);
            }
        }
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
    MallocMetaData *datap = (MallocMetaData*)((char*)oldp - sizeof(MallocMetaData));
    if (datap->cookies != free_list.cookies || datap->cookies != mmap_free_list.cookies)
    {
        exit(0xdeadbeef);
    }
    void* newp = NULL;
    if (size >= 128*KB)
    {
        /*
        potential errors:
            should we check if (datap->size > MY_MMAP_THRESHOLD) or (datap->size >= MY_MMAP_THRESHOLD)?
        */
        if (datap->size == size)
        {
            return oldp;
        }
        else
        {
            newp = smalloc(size); // which will use mmap() in this case.
            mmap_free_list.deleteFromFreeList(datap);
            mmap_free_list.insertToFreeList((MallocMetaData*)newp);
        }
    }
    else
    {
        MallocMetaData *datap_prev = free_list.findPreviousBuddy(datap);
        MallocMetaData *datap_next = free_list.findNextBuddy(datap);
        bool managed_to_contain = false;

        if (datap->cookies != free_list.cookies ||
            (datap_prev != NULL && datap_prev->cookies != free_list.cookies) ||
            (datap_next != NULL && datap_next->cookies != free_list.cookies))
        {
            // an overflow occured and someone used our data.
            exit(0xdeadbeef);
        }

        MallocMetaData* merged = NULL;
        if (datap_prev != NULL)
        {
            if (datap_next != NULL)
            {
                if ( ( ( datap_prev->size + datap->size) <= datap_next->size) && (!managed_to_contain) &&
                        ( ( datap_prev->size + sizeof(MallocMetaData) + datap->size) >= size ) )
                {
                    merged = free_list.mergeBlocks(datap_prev, datap);
                    managed_to_contain = true;
                }
                else if ( ( ( datap_prev->size + datap->size) > datap_next->size) && (!managed_to_contain) &&
                        ( ( datap_next->size + sizeof(MallocMetaData) + datap->size) >= size ) )
                {
                    merged = free_list.mergeBlocks(datap, datap_next);
                    managed_to_contain = true;
                }
                else if ( (!managed_to_contain) &&
                        ( ( datap_prev->size + sizeof(MallocMetaData) + datap_next->size + datap->size) >= size ) )
                {
                    merged = free_list.mergeBlocks(datap_prev, datap);
                    merged = free_list.mergeBlocks(merged, datap_next);
                    managed_to_contain = true;
                }
            }
            else
            {
                if ( (!managed_to_contain) &&
                        ( ( datap_prev->size + sizeof(MallocMetaData) + datap->size) >= size ) )
                {
                    merged = free_list.mergeBlocks(datap_prev, datap);
                    managed_to_contain = true;
                }
            }
        }
        else
        {
            if (datap_next != NULL)
            {
                if ( (!managed_to_contain) &&
                     ( ( datap_next->size + sizeof(MallocMetaData) + datap->size) >= size ) )
                {
                    merged = free_list.mergeBlocks(datap, datap_next);
                    managed_to_contain = true;
                }
            }
        }
        if (!managed_to_contain)
        {
            merged = free_list.findBlock(size);
            managed_to_contain = true;
        }
        newp = (void*)merged;
    }

    if(!newp)
    {
        return NULL;
    }
    if(memmove(newp, oldp, datap->size) != newp) 
    {
        return NULL;
    }
    sfree(oldp);
    return newp;
}

 size_t _num_free_blocks()
 {
    return free_list.num_free_blocks + mmap_free_list.num_allocated_blocks;
 }

 size_t _num_free_bytes()
 {
    return free_list.num_free_bytes + mmap_free_list.num_allocated_bytes;
 }

 size_t _num_allocated_blocks()
 {
    return (free_list.num_allocated_blocks + free_list.num_free_blocks
                + mmap_free_list.num_allocated_blocks + mmap_free_list.num_free_blocks); // look at function 7
 }
 
 size_t _num_allocated_bytes()
 {
    return (free_list.num_allocated_bytes + free_list.num_free_bytes
                + mmap_free_list.num_allocated_bytes + mmap_free_list.num_free_bytes);
 }

size_t _size_meta_data()
{
    return (sizeof(MallocMetaData));
}

size_t _num_meta_data_bytes()
{
    return (_size_meta_data() * _num_allocated_blocks());
}

// void DEBUG_PrintList()
// {
//     MallocMetaData *p = head_data.next;
//     printf("\nPrinting list:\n(size, addr)\n");
//     printf("HEAD <--> ");
//     for (; p != NULL; p = p->next)
//     {
//         if(!p->is_free)
//             printf("(%d, %p) <--> ", p->size, p->addr);
//     }
//     printf("NULL\n");

    
    
    
    
//     p = head_data.next;
//     printf("HEAD <--> ");
//     for (; p != NULL; p = p->next)
//     {
//         printf("(%d, %p) <--> ", p->size, p->addr);
//     }
//     printf("NULL\n");
// }
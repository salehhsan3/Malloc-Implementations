#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <cmath> // necessary for calculations

#define MAX_SIZE (1e8)
#define KB (1024)
#define MB (1024*1024)
#define MY_MMAP_THRESHOLD (128*KB)
#define DEFAULT_BUDDY_BLOCK (128*KB)
#define MIN_BUDDY_BLOCK (128)
#define BUDDY_BLOCKS_NUM 32
#define MAX_ORDER 10

static bool allocated_by_smalloc = false;
static bool allocated_by_scalloc = false;
static int  alloc_src_in_srealloc = 0;

void DEBUG_PrintList(); // to remove

class MallocMetaData
{
public:
    int cookies; // it's essential that the cookies are placed at the beginning of the block.
    size_t size;
    bool is_free;
    void* addr;
    MallocMetaData* next;
    MallocMetaData* prev;
    int allocation_src; /* can get 3 values: 0 := unallocated, 1 := allocated by scalloc, 2:= allocated by smalloc*/
    MallocMetaData(int cookies = 0, size_t size = 0, bool is_free = false, MallocMetaData* next = NULL, MallocMetaData* prev = NULL, int allocation_src = 0);
    ~MallocMetaData() = default;
    bool operator==(MallocMetaData& other);
    bool operator< (MallocMetaData& other);
    bool operator>=(MallocMetaData& other);
    bool operator> (MallocMetaData& other);
    bool operator<=(MallocMetaData& other);
};

MallocMetaData::MallocMetaData(int cookies, size_t size, bool is_free, MallocMetaData* next, MallocMetaData* prev, int allocation_src):
    cookies(cookies),
    size(size),
    is_free(is_free),
    next(next),
    prev(prev),
    allocation_src(allocation_src)
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
    double upper_size_bound = pow(2,MAX_ORDER)*MIN_BUDDY_BLOCK;
    if (size == upper_size_bound)
    {
        return MAX_ORDER;
    }
    
    for (int i = 0; i < (MAX_ORDER); i++)
    {
        double lower_bound = pow(2,i)*MIN_BUDDY_BLOCK;
        double upper_bound = pow(2,i+1)*MIN_BUDDY_BLOCK;
        if ( (size_t)(lower_bound) <= size && (size_t)upper_bound > size )
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
    if (this->head)
    {
        this->head->next = tail_node;
    }
    this->tail = tail_node;
    if (this->tail)
    {
        this->tail->prev = head_node;
    }
}

void MyList::insert(MallocMetaData* node)
{
    node->next = this->tail;
    node->prev = this->tail->prev;
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
    void removeMapping(MallocMetaData* block);
    bool isBlockContainable(MallocMetaData* block, size_t required_size);
};

void FreeList::initializeBuddySystem(MallocMetaData* head_nodes, MallocMetaData* tail_nodes)
{
    // allign allocations to start at a multiple of (32*128*KB) to ease calculations later
    void* curr_prog_break = sbrk(0);
    size_t align_to_num = (32*128*KB);
    size_t alloc_alignment_size = align_to_num - ( ((size_t)curr_prog_break) % align_to_num ); // check correctness!!!!!!!!!!!
    this->wasted_block = curr_prog_break;
    this->wasted_block_size = alloc_alignment_size;
    this->wasted_block = sbrk(alloc_alignment_size);


    // allocate the new aray of size (32*128*KB)
    void* new_list = sbrk(32*128*KB);
    for (int i = 0; i < BUDDY_BLOCKS_NUM; i++)
    {
        MallocMetaData* curr = (MallocMetaData*)((char*)new_list + (i*DEFAULT_BUDDY_BLOCK));
        // MallocMetaData* next = (MallocMetaData*)((char*)new_list + ((i+1)*DEFAULT_BUDDY_BLOCK));
        // MallocMetaData* prev = (MallocMetaData*)((char*)new_list + ((i-1)*DEFAULT_BUDDY_BLOCK));
        curr->size = (128*KB) - sizeof(MallocMetaData);
        curr->is_free = true;
        curr->cookies = this->cookies;
        curr->addr = (void*)((char*)curr + sizeof(MallocMetaData));
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
    this->head->next = tail_node;
    this->tail = tail_node;
    this->tail->prev = head_node;
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
        for (MallocMetaData* curr = this->orders_list[i].head->next; curr != this->orders_list[i].tail; curr = curr->next)
        {
            if (curr->cookies != this->cookies)
            {
                exit(0xdeadbeef);
            }
            
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
    size_t new_size = (current_size - sizeof(MallocMetaData))/2; // the size of the block after splitting
    bool inserted = false;
    while ( min_size <= new_size )
    {
        current_size = curr_block->size;
        new_size = (current_size - sizeof(MallocMetaData))/2;
        if ( (current_size + sizeof(MallocMetaData)) > MIN_BUDDY_BLOCK && new_size >= min_size)
        {
            curr_block->size = new_size;
            /*potential errors:
                added sizeof(MallocMetaData)
            */
            new_block = (MallocMetaData*)((char*)curr_block + new_size + sizeof(MallocMetaData) );
            new_block->cookies = curr_block->cookies;
            new_block->size = new_size;
            new_block->is_free = true;
            new_block->addr = (void*)((char*)new_block + sizeof(MallocMetaData));
            int index_old = getOrderFromSize( current_size + sizeof(MallocMetaData) );
            int index_new = getOrderFromSize( new_size + sizeof(MallocMetaData) );
            // int index_old = getOrderFromSize( current_size );
            // int index_new = getOrderFromSize( new_size );
            this->orders_list[index_old].removeBlock(curr_block);
            this->orders_list[index_new].insert(curr_block);
            this->orders_list[index_new].insert(new_block);
            current_size = new_size;
            inserted = true;
            this->num_free_blocks += 1; //+2?
            this->num_free_bytes -= sizeof(MallocMetaData);
        }
        else if(!inserted || current_size <= MIN_BUDDY_BLOCK)
        {
            // can't split a block with the minimum size.
            /* potential errors: here!!!*/
            return curr_block;
        }
    }
    
    return curr_block;
}

MallocMetaData* FreeList::mergeBlocks(MallocMetaData* prev, MallocMetaData* curr)
{   
    if ( (prev->size + sizeof(MallocMetaData) + curr->size) > DEFAULT_BUDDY_BLOCK )
    {
        return NULL;
    }
    
    int index_prev = getOrderFromSize(prev->size + sizeof(MallocMetaData));
    int index_curr = getOrderFromSize(curr->size + sizeof(MallocMetaData));
    this->orders_list[index_prev].removeBlock(prev);
    this->orders_list[index_curr].removeBlock(curr);
    prev->size += (curr->size + sizeof(MallocMetaData));
    int new_index = getOrderFromSize( prev->size + sizeof(MallocMetaData));
    this->orders_list[new_index].insert(prev);
    this->num_free_blocks -= 1;
    this->num_free_bytes += sizeof(MallocMetaData);
    return prev;
}

MallocMetaData* FreeList::findNextBuddy(MallocMetaData* block)
{
    if(!block)
    {
        return NULL;
    }
    for (int i = 0; i < (MAX_ORDER + 1); i++)
    {
        for (MallocMetaData* curr = this->orders_list[i].head->next; curr != this->orders_list[i].tail; curr = curr->next)
        {
            MallocMetaData* end_of_block = (MallocMetaData*)((char*)block + sizeof(MallocMetaData) + block->size );
            if ( ( end_of_block == curr ) && curr->is_free )
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
            MallocMetaData* end_of_curr = (MallocMetaData*)((char*)curr + sizeof(MallocMetaData) + curr->size);
            if ( ( end_of_curr == block ) && curr->is_free )
            {
                return curr;
            }  
        }
    }
    return NULL;
}

void FreeList::insertToFreeList(MallocMetaData* block)
{
    block->next = this->tail;
    block->prev = this->tail->prev;
    this->tail->prev = block;
    this->tail->prev->next = block;
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
    void* allocation;
    int case_num = 0;
    if ( ( allocated_by_scalloc || (alloc_src_in_srealloc == 1) ) && (size >= 2*MB) )
    {
        case_num = 1;
    }
    else if ( ( allocated_by_smalloc || (alloc_src_in_srealloc == 2) ) && (size >= 4*MB) )
    {
        case_num = 2;
    }
    allocated_by_scalloc = false;
    allocated_by_smalloc = false;
    alloc_src_in_srealloc = 0;
    switch (case_num)
    {
    case 1:
        allocation = mmap(NULL, (size + sizeof(MallocMetaData)), (PROT_EXEC | PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB), -1, 0); // correct flags / prot?
        ((MallocMetaData*)allocation)->allocation_src = 1;
        break;

    case 2:
        allocation = mmap(NULL, (size + sizeof(MallocMetaData)), (PROT_EXEC | PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB), -1, 0); // correct flags / prot?
        ((MallocMetaData*)allocation)->allocation_src = 2;
        break;    
    
    default:
        allocation = mmap(NULL, (size + sizeof(MallocMetaData)), (PROT_EXEC | PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0); // correct flags / prot?
        break;
    }
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

    // this->num_allocated_bytes += (size + sizeof(MallocMetaData));
    this->num_allocated_bytes += (size);
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

        // if ( (found->size) > (2*size) ) /* potential errors: maybe wrong condition*/
        if ( (found->size - sizeof(MallocMetaData)) > 2*size )
        {
            MallocMetaData* split_block = this->splitBlock(found, size);
            /* potential errors*/
            // things to consider: when splitting the blocks I should change the number of free_bytes, block...
            // potential errors: numerical calculations with size, etc.
            split_block->is_free = false;
            this->num_free_blocks -= 1;
            this->num_allocated_blocks += 1;
            this->num_free_bytes -= (split_block->size);
            this->num_allocated_bytes += (split_block->size);
            return split_block->addr;
        }
        else
        {
            found->is_free = false;
            this->num_allocated_blocks += 1;
            this->num_allocated_bytes += found->size;
            this->num_free_blocks -= 1;
            this->num_free_bytes -= found->size;
            return found->addr;
        }
    }
    return NULL;
}

void FreeList::removeMapping(MallocMetaData* block)
{
    for (MallocMetaData* curr = this->head->next; curr != this->tail; curr = curr->next)
    {
        if (block == curr)
        {
            curr->prev->next = curr->next;
            curr->next->prev = curr->prev;
        }
    }   
}

bool FreeList::isBlockContainable(MallocMetaData* block, size_t required_size)
{
    MallocMetaData* next_buddy = this->findNextBuddy(block);
    MallocMetaData* prev_buddy = this->findPreviousBuddy(block);
    size_t accumulated_size = block->size;
    while ( required_size > accumulated_size )
    {
        if ( next_buddy && prev_buddy && (prev_buddy->size <= next_buddy->size) )
        {
            accumulated_size += (prev_buddy->size);
            prev_buddy = this->findPreviousBuddy(prev_buddy);
        }
        else if ( next_buddy && prev_buddy && ( prev_buddy->size > next_buddy->size ) )
        {
            accumulated_size += (next_buddy->size);
            next_buddy = this->findNextBuddy(next_buddy);
        }
        else if ( next_buddy && (!prev_buddy) )
        {
            accumulated_size += (next_buddy->size);
            next_buddy = this->findNextBuddy(next_buddy);
        }
        else if ( (!next_buddy) && prev_buddy )
        {
            accumulated_size += (prev_buddy->size);
            prev_buddy = this->findPreviousBuddy(prev_buddy);
        }
    }
    return (accumulated_size >= required_size);
}

// the following elements are allocated on the stack!
static MallocMetaData head_datas[BUDDY_BLOCKS_NUM+1];
static MallocMetaData tail_datas[BUDDY_BLOCKS_NUM+1];
static MallocMetaData list_head_node = MallocMetaData();
static MallocMetaData mmap_head_node = MallocMetaData();
static MallocMetaData list_tail_node = MallocMetaData();
static MallocMetaData mmap_tail_node = MallocMetaData();
static FreeList free_list = FreeList(&list_head_node, &list_tail_node, head_datas, tail_datas);
static FreeList mmap_free_list = FreeList(&mmap_head_node, &mmap_tail_node, head_datas, tail_datas);
static bool buddy_system_init = false;

void *smalloc(size_t size)
{
    if (!buddy_system_init)
    {
        free_list.initializeBuddySystem(head_datas, tail_datas);
        buddy_system_init = true;
    }
    
    if (size <= 0 || size > MAX_SIZE)
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
        allocated_by_smalloc = true;
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
    allocated_by_scalloc = true;
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
    MallocMetaData* merged = datap;
    if (datap->cookies != free_list.cookies || datap->cookies != mmap_free_list.cookies)
    {
        // an overflow occured and someone used our data.
        exit(0xdeadbeef);
    }
    if (datap->is_free)
    {
        return; // block is already free!
    }
    
    if (datap->size >= MY_MMAP_THRESHOLD)
    {
        /*
        potential errors:
            should we check if (datap->size > MY_MMAP_THRESHOLD) or (datap->size >= MY_MMAP_THRESHOLD)?
            according to the notes in section 3, we shouldn't consider munmapped areas as freed
        */
        mmap_free_list.num_allocated_blocks -= 1;
        mmap_free_list.num_allocated_bytes -= datap->size;
        datap->is_free = true;
        MallocMetaData* found = mmap_free_list.findBlock(datap->size);
        if (found)
        {
            mmap_free_list.removeMapping(datap);
        }
        munmap(datap, datap->size + sizeof(MallocMetaData));
        // wasnt very clear on what we should do with unmapped blocks, check it.
    }
    else
    {
        merged->is_free = true;
        free_list.num_free_blocks += 1;
        free_list.num_free_bytes += merged->size;
        free_list.num_allocated_blocks -= 1;
        free_list.num_allocated_bytes -= merged->size;

        MallocMetaData *datap_prev = free_list.findPreviousBuddy(merged);
        MallocMetaData *datap_next = free_list.findNextBuddy(merged);
        while ( (datap_next || datap_prev) && (merged))
        {
            if (datap->cookies != free_list.cookies ||
            (datap_prev != NULL && datap_prev->cookies != free_list.cookies) ||
            (datap_next != NULL && datap_next->cookies != free_list.cookies))
            {
                // an overflow occured and someone used our data.
                exit(0xdeadbeef);
            }
            if (datap_prev != NULL)
            {
                merged = free_list.mergeBlocks(datap_prev, merged);
                if (datap_next != NULL)
                {
                    merged = free_list.mergeBlocks(merged, datap_next);
                }
            }
            else
            {
                if (datap_next != NULL)
                {
                    merged = free_list.mergeBlocks(merged, datap_next);
                }
            }
            datap_prev = free_list.findPreviousBuddy(merged);
            datap_next = free_list.findNextBuddy(merged);
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
    bool merged_blocks = false;
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
            alloc_src_in_srealloc = datap->allocation_src;
            newp = smalloc(size); // which will use mmap() in this case.
            mmap_free_list.deleteFromFreeList(datap);
            mmap_free_list.insertToFreeList((MallocMetaData*)newp);
        }
    }
    else
    {
        MallocMetaData *datap_prev = free_list.findPreviousBuddy(datap);
        MallocMetaData *datap_next = free_list.findNextBuddy(datap);
        bool managed_to_contain = free_list.isBlockContainable(datap, size);
        MallocMetaData* merged = datap;

        if (datap->cookies != free_list.cookies ||
            (datap_prev != NULL && datap_prev->cookies != free_list.cookies) ||
            (datap_next != NULL && datap_next->cookies != free_list.cookies))
        {
            // an overflow occured and someone used our data.
            exit(0xdeadbeef);
        }
        
        if (managed_to_contain)
        {
            while ( merged && (merged->size < size) )
            {
                if ( datap_next && datap_prev && (datap_prev->size <= datap_next->size) )
                {
                    free_list.num_allocated_bytes -= merged->size;
                    free_list.num_free_bytes -= merged->size;
                    merged = free_list.mergeBlocks(datap_prev, merged);
                    merged->is_free = false;
                    free_list.num_free_bytes -= (sizeof(MallocMetaData));
                    free_list.num_allocated_bytes += merged->size;
                    merged_blocks = true;
                }
                else if ( datap_next && datap_prev && ( datap_prev->size > datap_next->size ) )
                {
                    free_list.num_allocated_bytes -= merged->size;
                    free_list.num_free_bytes -= merged->size;
                    merged = free_list.mergeBlocks(merged, datap_next);
                    merged->is_free = false;
                    free_list.num_free_bytes -= (sizeof(MallocMetaData));
                    free_list.num_allocated_bytes += merged->size;
                    merged_blocks = true;
                }
                else if ( datap_next && (!datap_prev) )
                {
                    free_list.num_allocated_bytes -= merged->size;
                    free_list.num_free_bytes -= merged->size;
                    merged = free_list.mergeBlocks(merged, datap_next);
                    merged->is_free = false;
                    free_list.num_free_bytes -= (sizeof(MallocMetaData));
                    free_list.num_allocated_bytes += (merged->size);
                    merged_blocks = true;
                }
                else if ( (!datap_next) && datap_prev )
                {
                    free_list.num_allocated_bytes -= merged->size;
                    free_list.num_free_bytes -= merged->size;
                    merged = free_list.mergeBlocks(datap_prev, merged);
                    merged->is_free = false;
                    free_list.num_free_bytes -= (sizeof(MallocMetaData));
                    free_list.num_allocated_bytes += (merged->size);
                    merged_blocks = true;
                } 
                datap_prev = free_list.findPreviousBuddy(merged);
                datap_next = free_list.findNextBuddy(merged);
            }   
        }
        else
        {
            merged = free_list.findBlock(size);
            managed_to_contain = true;
        }
        newp = merged->addr;
    }

    if(!newp)
    {
        return NULL;
    }
    if(memmove(newp, oldp, datap->size) != newp) 
    {
        return NULL;
    }
    if ( (!merged_blocks) && (newp != oldp) )
    {
        sfree(oldp);
    }
    return newp;
}

size_t _num_free_blocks()
{
    return free_list.num_free_blocks + mmap_free_list.num_free_blocks;
}

size_t _num_free_bytes()
{
    return free_list.num_free_bytes + mmap_free_list.num_free_bytes;
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

// size_t _num_free_blocks()
// {
// return free_list.num_free_blocks + mmap_free_list.num_free_blocks;
// }

// size_t _num_free_bytes()
// {
// return free_list.num_free_bytes + mmap_free_list.num_free_bytes;
// }

// size_t _num_allocated_blocks()
// {
// return (free_list.num_allocated_blocks + mmap_free_list.num_allocated_blocks ); // look at function 7
// }

// size_t _num_allocated_bytes()
// {
// return (free_list.num_allocated_bytes + mmap_free_list.num_allocated_bytes );
// }

size_t _size_meta_data()
{
    return (sizeof(MallocMetaData));
}

size_t _num_meta_data_bytes()
{
    return (_size_meta_data() * _num_allocated_blocks());
}

void DEBUG_PrintList()
{
    int freed = 0;
    int allocated = 0;
    for (int i = 0; i < (MAX_ORDER+1); i++)
    {
        int i_freed = 0;
        int i_allocated = 0;

        MallocMetaData *p = free_list.orders_list[i].head->next;
        printf("\nPrinting list with free:\n(size, addr)\n");
        printf("HEAD <--> ");
        for (; p != free_list.orders_list[i].tail; p = p->next)
        {
            if(p->is_free)
            {
                printf("(%ld, %p) <--> ", p->size, p->addr);
                freed++;
                i_freed++;
            }
        }
        printf("TAIL(freed:%d, order:%d)\n",i_freed,i);
        printf("--------------------------------------------------------------------------\n");

        p = free_list.orders_list[i].head->next;
        printf("\nPrinting list with allocated:\n(size, addr)\n");
        printf("HEAD <--> ");
        for (; p != free_list.orders_list[i].tail; p = p->next)
        {
            if(!p->is_free)
            {
                printf("(%ld, %p) <--> ", p->size, p->addr);
                allocated++;
                i_allocated++;
            }       
        }
        printf("TAIL(allocated:%d, order:%d)\n",i_allocated,i);
        printf("--------------------------------------------------------------------------\n");
    }
        printf("Total Free: %d, Total allocated: %d\n", freed, allocated);
        printf("===========================================================================\n");
}
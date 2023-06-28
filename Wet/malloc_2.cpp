#include <unistd.h>
#include <string.h>

#define MAX_SIZE (1e8)

class MallocMetaData
{
public:
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
    MallocMetaData(size_t size = 0, bool is_free = false, MallocMetaData* next = NULL, MallocMetaData* prev = NULL);
    ~MallocMetaData() = default;
};

MallocMetaData::MallocMetaData(size_t size, bool is_free, MallocMetaData* next, MallocMetaData* prev):
    size(size),
    is_free(is_free),
    next(next),
    prev(prev)
{}

class FreeList
{
public:
    MallocMetaData *head;
    size_t num_allocated_bytes;
    size_t num_free_bytes;
    size_t num_allocated_blocks;
    size_t num_free_blocks;
    FreeList(MallocMetaData* head_data);
    ~FreeList() = default;
    MallocMetaData* SearchBlock(size_t size);
    void AddMetaData_Block(MallocMetaData* data, size_t size);
};

FreeList::FreeList(MallocMetaData* head)
{
    this->head = head;
    this->num_free_blocks = 0;
    this->num_allocated_blocks = 0;
    this->num_free_bytes = 0;
    this->num_allocated_bytes = 0;
}

MallocMetaData* FreeList::SearchBlock(size_t size)
{
    for ( MallocMetaData *curr = this->head->next; curr != NULL; curr = curr->next)
    {
        if (curr->size >= size && curr->is_free == true)
        {
            return curr;
        } 
    }
    return NULL;
}

void FreeList::AddMetaData_Block(MallocMetaData* data, size_t size)
{
    data->size = size;
    data->is_free = false;

    for ( MallocMetaData *curr = this->head; curr != NULL; curr = curr->next)
    {
        if (curr->next == NULL)
        {
            curr->next = data;
            data->prev = curr;
            data->next = NULL;
            break;
        } 
    }
}

static MallocMetaData head_data = MallocMetaData();
static FreeList free_list = FreeList(&head_data);

void *smalloc(size_t size)
{
    if (size <= 0 || size > MAX_SIZE)
    {    
        return NULL;
    }
    MallocMetaData* wanted_block = free_list.SearchBlock(size); 
    if(wanted_block == NULL)
    {
        // allocate a proper block
        void *old_porogram_break = sbrk(0);
        void* allocation = sbrk(size + sizeof(MallocMetaData));
        free_list.num_allocated_bytes += size;
        free_list.num_allocated_blocks++; 
        if(allocation == (void*)(-1))
        {
            return NULL;
        }

        free_list.AddMetaData_Block( static_cast<MallocMetaData*>(old_porogram_break) , size);
        return (void*)( (char*)(old_porogram_break) + sizeof(MallocMetaData));
    }
    else
    {
        // reuse previous block
        wanted_block->is_free = false;
        free_list.num_allocated_blocks++;
        free_list.num_free_blocks--;
        free_list.num_allocated_bytes += wanted_block->size;
        free_list.num_free_bytes -= wanted_block->size;
        return (void*) ( (char*)(wanted_block) + sizeof(MallocMetaData));
    }
    
}

void *scalloc(size_t num, size_t size)
{
    if (num <= 0 || size <= 0 || (size*num) > MAX_SIZE)
    {
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
    
    MallocMetaData *p_metadata = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));
    if(p_metadata->is_free == true)
    {
        return;
    }
    else
    {
        free_list.num_allocated_blocks--;
        free_list.num_free_blocks++;
        free_list.num_allocated_bytes -= p_metadata->size;
        free_list.num_free_bytes += p_metadata->size;
        p_metadata->is_free = true;
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
    MallocMetaData *p_metadata = (MallocMetaData*)((char*)oldp - sizeof(MallocMetaData));
    if( size <= p_metadata->size )
    {
        return oldp;
    }
    void* allocation = smalloc(size);
    if(allocation == NULL)
    {
        return NULL;
    }
    if(memmove(allocation, oldp, p_metadata->size) != allocation) 
    {
        return NULL;
    }
    sfree(oldp);
    return allocation;
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
    return free_list.num_allocated_blocks + free_list.num_free_blocks; 
}

size_t _num_allocated_bytes()
{
    return free_list.num_allocated_bytes + free_list.num_free_bytes;
}

size_t _size_meta_data()
{
    return (sizeof(MallocMetaData));
}

size_t _num_meta_data_bytes()
{
    return (_size_meta_data() * _num_allocated_blocks());
}
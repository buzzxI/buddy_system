#include <csapp.h>

#define MAX_LEVEL (10)
#define PAGE_ORDER (10)
// page size is 1KB
#define PAGE_SIZE (1 << PAGE_ORDER)
// total mem space size is 1 GB
#define MEM_SPACE (1 << 30)

#define MEM_CHECK

typedef unsigned int u32;
typedef unsigned long long u64;
typedef struct page page_t;
typedef struct list_node list_node_t;
typedef struct free_list free_list_t;
typedef struct mem_pool mem_pool_t;

struct list_node {
    list_node_t* next;
    list_node_t* pre;
};

// meta data of page
struct page {
    list_node_t node;
    int order; 
    int allocated;
};

struct free_list {
    list_node_t head;
    u64 node_cnt;
};

struct mem_pool {
    page_t* meta_start;
    void* mem_start;
    free_list_t lists[MAX_LEVEL];
    u64 page_num;
    void* end;
};

static mem_pool_t pool;

static inline void init_list_node(list_node_t* node) {
    node->next = node;
    node->pre = node;
}

static inline void append_node(list_node_t* node, list_node_t* head) {
    // node to list
    node->next = head->next;
    node->pre = head;
    // list to node
    head->next->pre = node;
    head->next = node;
}

static inline void detach_node(list_node_t* node) {
    // list to node
    node->pre->next = node->next;
    node->next->pre = node->pre;

    // node to list
    node->next = NULL;
    node->pre = NULL;
}

static const unsigned int b[] = {0x2, 0xc, 0xf0, 0xff00, 0xffff0000};
static const unsigned int s[] = {1, 2, 4, 8, 16};
static int get_sign_bit(int num) {
    if (num == 0) return -1;
    int rst = 0;
    int i;
    for (i = 4; i >= 0; i--) {
        if (num & b[i]) {
            num >>= s[i];
            rst |= s[i];
        }
    }
    return rst;
}

page_t* get_buddy(page_t* page);
void init_memory();

void free_page(page_t* page);
page_t* merge_page(page_t* page);

page_t* split_page(page_t* page, u32 order);

void* allocate(u32 size);
void deallocate(void* ptr);

void each_level_page_check();
void all_pages_check();
void page_count_check();

int main(int argc, char** argv) {
    init_memory();
#ifdef MEM_CHECK
    int i;
    for (i = 0; i < 10; i++) {
        each_level_page_check();
        all_pages_check();
        each_level_page_check();
    }
    page_count_check();
#endif
    return 0;
}

void init_memory() {
    // cal page cnt
    void* start_addr = Malloc(MEM_SPACE * sizeof(char));
    u64 page_num = MEM_SPACE / (PAGE_SIZE + sizeof(page_t));
    pool.meta_start = start_addr;
    pool.page_num = page_num;
    pool.mem_start = (void*)((u64)pool.meta_start + pool.page_num * sizeof(page_t));
    
    fprintf(stdout, "start: %p, mem_start: %p, end: %p, page num: %llu\n", start_addr, pool.mem_start, start_addr + MEM_SPACE, pool.page_num);

    fprintf(stdout, "meta_num: %llu, mem_num; %llu\n", ((u64)pool.mem_start - (u64)pool.meta_start) / sizeof(page_t), ((u64)pool.meta_start + MEM_SPACE - (u64)pool.mem_start) / PAGE_SIZE);

    int i;
    // init free list
    for (i = 0; i < MAX_LEVEL; i++) {
        pool.lists[i].node_cnt = 0;
        init_list_node(&pool.lists[i].head); 
    }

    // init page
    for (i = 0; i < page_num; i++) {
        page_t* page = pool.meta_start + i;
        page->allocated = 1;
        page->order = 0;
    }

    for (i = 0; i < page_num; i++) {
        page_t* page = pool.meta_start + i;
        free_page(page);
    }
}

void* allocate(u32 size) {
    if (size == 0) return NULL;
    // get page num(round up)
    u32 page_num = (size + PAGE_SIZE - 1)/ PAGE_SIZE;
    // get order by round up
    u32 high_bit = get_sign_bit(page_num);
    if (page_num > 1 << high_bit) high_bit++;    
    if (high_bit >= MAX_LEVEL) {
        fprintf(stderr, "cannot allocate memory larger than limit:%d\n", 1 << (MAX_LEVEL - 1));
        return NULL;
    }
    page_t* rst_page = NULL;
    int current_order;
    for (current_order = high_bit; current_order < MAX_LEVEL && rst_page == NULL; current_order++) {
        if (!pool.lists[current_order].node_cnt) continue;
        rst_page = (page_t*)pool.lists[current_order].head.next;
        detach_node(&rst_page->node);
        pool.lists[current_order].node_cnt--;
        rst_page = split_page(rst_page, high_bit);
    }
    if (rst_page == NULL) return NULL;
    page_t* buddy = get_buddy(rst_page);
    rst_page->allocated = 1;
    u32 page_idx = rst_page - pool.meta_start;
    return pool.mem_start + page_idx * PAGE_SIZE;
}

void deallocate(void* ptr) {
    if (ptr == NULL) return;
    u32 page_idx = (ptr - pool.mem_start) / PAGE_SIZE;
    free_page(pool.meta_start + page_idx);  
}

// split @param: page into @param:size
page_t* split_page(page_t* page, u32 order) {
    while (page->order > order) {
        page->order--;
        page_t* buddy = get_buddy(page);
        buddy->order = page->order;
        append_node(&buddy->node, &pool.lists[buddy->order].head);
        pool.lists[buddy->order].node_cnt++;
    }
    return page;
}

page_t* merge_page(page_t* page) {
    page_t* buddy = get_buddy(page);
    while (buddy != NULL && !buddy->allocated && buddy->order == page->order && page->order < MAX_LEVEL - 1) {
        detach_node(&buddy->node);
        pool.lists[buddy->order].node_cnt--;
        if (buddy < page) page = buddy;
        page->order++;
        buddy = get_buddy(page);
    }
    return page;
}

void free_page(page_t* page) {
    page->allocated = 0;
    page = merge_page(page);
    pool.lists[page->order].node_cnt++;
    append_node(&page->node, &pool.lists[page->order].head);
}

page_t* get_buddy(page_t* page) {
    u64 sign_bit = 1 << (page->order + PAGE_ORDER);
    u64 idx = ((u64)page - (u64)pool.meta_start) / sizeof(page_t);
    u64 page_addr = (u64)pool.mem_start + PAGE_SIZE * idx;
    u64 buddy_addr = page_addr ^ sign_bit;
    if (buddy_addr < (u64)pool.mem_start || buddy_addr >= (u64)pool.meta_start + MEM_SPACE - PAGE_SIZE) return NULL;
    u64 buddy_idx = ((buddy_addr - (u64)pool.mem_start) / PAGE_SIZE);
    page_t* buddy = pool.meta_start + buddy_idx;
    if (page->order != buddy->order) {
        // fprintf(stdout, "page:idx: %lld:order:%d:buddy:idx:%lld:order:%d\n", idx, page->order, buddy_idx, buddy->order);
    }
    return pool.meta_start + buddy_idx;
}

void each_level_page_check() {
    // check allocation of each level of page
    int i;
    void* mem_block;
    for (i = 0; i < MAX_LEVEL; i++) {
        u64 ori_size = pool.lists[i].node_cnt;
        u64 size = PAGE_SIZE * (1 << i);
        mem_block = allocate(size);
        memset(mem_block, 'b', size);
        deallocate(mem_block);
        if (ori_size != pool.lists[i].node_cnt) fprintf(stderr, "block lost: %d\n", i);
    }
    for (i = MAX_LEVEL - 1; i >= 0; i--) {
        u64 ori_size = pool.lists[i].node_cnt;
        u64 size = PAGE_SIZE * (1 << i);
        mem_block = allocate(size);
        memset(mem_block, 'u', size);
        deallocate(mem_block);
        if (ori_size != pool.lists[i].node_cnt) fprintf(stderr, "block lost: %d\n", i);
    }
    void* blocks[MAX_LEVEL];
    for (i = 0; i < MAX_LEVEL; i++) {
        u64 size = PAGE_SIZE * (1 << i);
        blocks[i] = allocate(size);
        memset(blocks[i], 'd', size);
    }
    for (i = 0; i < MAX_LEVEL; i++) deallocate(blocks[i]);
    fprintf(stdout, "each:level:check:done\n");
}

void all_pages_check() {
    int i;
    void* mem_block;
    for (i = 0; i < pool.page_num; i++) {
        mem_block = allocate(PAGE_SIZE);
        memset(mem_block, 'd', PAGE_SIZE);
    }
    for (i = 0; i < pool.page_num; i++) deallocate(pool.mem_start + PAGE_SIZE * i);
    fprintf(stdout, "all:memory:check:done\n");
}

void page_count_check() {
    u64 page_cnt = 0;
    int i;
    for (i = 0; i < MAX_LEVEL; i++) page_cnt += (1 << i) * pool.lists[i].node_cnt;
    if (page_cnt != pool.page_num) fprintf(stdout, "lost:pages:i:%d\n", i);
}
# A Simple Buddy System 

this is a simple dynamic memory allocator based on [Buddy System](https://en.wikipedia.org/wiki/Buddy_system)

this project is innovated from buddy system from [chcore](https://gitee.com/ipads-lab/chcore-lab-v2)

`struct mem_pool` is the top manager, which manages multiple free list(`struct free list`)

pages with orders(`struct page`) are linked in free list

`struct list_node` is used to link pages 


#include "waypoints.h"
#include "interval-tree/rbtree.h"
#include "interval-tree/interval_tree_generic.h"

#include <malloc.h>

#define BUF_MAP_SIZE (1 << 16)

FUZZFACTORY_DSF_NEW(__afl_buf_start_dsf, BUF_MAP_SIZE, FUZZFACTORY_REDUCER_MAX, 0);
FUZZFACTORY_DSF_NEW(__afl_buf_end_dsf, BUF_MAP_SIZE, FUZZFACTORY_REDUCER_MAX, 0);

struct alloc_tree_node {

  struct rb_node rb;
  uintptr_t start;
  uintptr_t last;
  uintptr_t alloc_site;
  uintptr_t __subtree_last;

};

#define START(node) ((node)->start)
#define LAST(node) ((node)->last)

INTERVAL_TREE_DEFINE(struct alloc_tree_node, rb, uintptr_t, __subtree_last,
                     START, LAST, static, alloc_tree)

static struct rb_root root = RB_ROOT;

int __afl_buf_alloc_tree_search(uintptr_t query, uintptr_t* start, uintptr_t* end, uintptr_t* alloc_site) {
  struct alloc_tree_node* node = alloc_tree_iter_first(&root, query, query);
  if (!node) return 0;
  *start = node->start;
  *end = node->last;
  *alloc_site = node->alloc_site;
  return 1;
}

void __afl_buf_alloc_tree_insert(uintptr_t start, uintptr_t end, uintptr_t alloc_site) {
  struct alloc_tree_node* node = calloc(sizeof(struct alloc_tree_node), 1);
  node->start = start;
  node->last = end;
  node->alloc_site = alloc_site;
  alloc_tree_insert(node, &root);
}

void __afl_buf_alloc_tree_remove(uintptr_t start, uintptr_t end) {
  struct alloc_tree_node* prev_node = alloc_tree_iter_first(&root, start, end);
  while (prev_node) {
    struct alloc_tree_node* n = alloc_tree_iter_next(prev_node, start, end);
    alloc_tree_remove(prev_node, &root);
    prev_node = n;
  }
}

void __afl_buf_access(uintptr_t ptr, uint32_t size) {
  uintptr_t start, end, alloc_site;
  if (!__afl_buf_alloc_tree_search(ptr, &start, &end, &alloc_site)) return;
  uintptr_t k = (uintptr_t)__builtin_return_address(0);
  k = (k >> 4) ^ (k << 8) ^ alloc_site;
  k &= BUF_MAP_SIZE - 1;
  FUZZFACTORY_DSF_MAX(__afl_buf_start_dsf, k, (uint32_t)(start - ptr));
  FUZZFACTORY_DSF_MAX(__afl_buf_end_dsf, k, (uint32_t)((ptr + size) - end));
}

void __afl_buf_handle_malloc(uint32_t k, uintptr_t ptr, uintptr_t len) {
  __afl_buf_alloc_tree_insert(ptr, ptr + len, k);
}

void __afl_buf_handle_calloc(uint32_t k, uintptr_t ptr, uintptr_t elem_len, uintptr_t elem_cnt) {
  __afl_buf_alloc_tree_insert(ptr, ptr + elem_len * elem_cnt, k);
}

void __afl_buf_handle_realloc(uint32_t k, uintptr_t old_ptr, uintptr_t ptr, size_t len) {
  __afl_buf_alloc_tree_remove(old_ptr, old_ptr);
  __afl_buf_alloc_tree_insert(ptr, ptr + len, k);
}

void __afl_buf_handle_free(uintptr_t ptr) {
  __afl_buf_alloc_tree_remove(ptr, ptr);
}

/*
int __afl_buf_wrap_posix_memalign(void** ptr, size_t align, size_t len) {

}

void*__afl_buf_wrap_ memalign(size_t align, size_t len) {

}
*/

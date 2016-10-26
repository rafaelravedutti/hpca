#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHUNK 1024      /* read 1024 bytes at a time */

/* L1 cache parameters */
#define L1_SIZE                 (64 * 1024)
#define L1_WAYS                 4
#define L1_BLOCK_SIZE           64
#define L1_LATENCY              2

/* L2 cache parameters */
#define L2_SIZE                 (2 * 1024 * 1024)
#define L2_WAYS                 8
#define L2_BLOCK_SIZE           64
#define L2_LATENCY              4

/* Fetch return codes */
#define FETCH_HIT               1
#define FETCH_MISS              2

/* DRAM latency */
#define DRAM_LATENCY            150

/* PC based stride prefetcher table lines */
#define PC_STRIDE_TABLE_LINES   64

/* Policies:
   - Write Back with Write-Allocate
   - Least Recently Used (LRU) replacement
   - Non Inclusve
*/

struct cache_entry {
  unsigned long tag;
  unsigned long cycle;
  int valid;
  int dirty;
};

struct pc_stride_entry {
  unsigned long tag;
  unsigned long last_address;
  unsigned int stride;
  unsigned char state; /* Init - Trans - Steady - No Pred */
};

static struct cache_entry l1_cache[L1_SIZE / L1_BLOCK_SIZE][L1_WAYS];
static struct cache_entry l2_cache[L2_SIZE / L2_BLOCK_SIZE][L2_WAYS];
static struct pc_stride_entry pc_stride_table[PC_STRIDE_TABLE_LINES];

int get_least_recently_used(struct cache_entry entries[], unsigned int nways) {
  int result = 0;
  unsigned long long min_cycle;
  unsigned int i;

  min_cycle = entries[0].cycle;

  for(i = 1; i < nways; ++i) {
    if(entries[i].cycle < min_cycle) {
      result = i;
      min_cycle = entries[i].cycle;
    }
  }

  return result;
}
 
int fetch_data_from_l1(unsigned long address, unsigned int *way) {
  unsigned long tag, index, offset;
  unsigned int i;

  tag = address >> 16;
  index = (address >> 6) & 0x3FF;
  offset = address & 0x3F;

  for(i = 0; i < L1_WAYS; ++i) {
    if(l1_cache[index][i].valid == 1 && l1_cache[index][i].tag == tag) {
      *way = i;
      return FETCH_HIT;
    }
  }

  return FETCH_MISS;
}

int fetch_data_from_l2(unsigned long address, unsigned int *way) {
  unsigned long tag, index, offset;
  unsigned int i;

  tag = address >> 22;
  index = (address >> 6) & 0x7FFF;
  offset = address & 0x3F;

  for(i = 0; i < L2_WAYS; ++i) {
    if(l2_cache[index][i].valid == 1 && l2_cache[index][i].tag == tag) {
      *way = i;
      return FETCH_HIT;
    }
  }

  return FETCH_MISS;
}

void write_l1_data(unsigned long address, int way, int dirty, unsigned long long cycle) {
  unsigned long tag, index, offset;

  tag = address >> 16;
  index = (address >> 6) & 0x3FF;
  offset = address & 0x3F;

  if(way < 0) {
    way = get_least_recently_used(l1_cache[index], L1_WAYS);
  }

  l1_cache[index][way].valid = 1;
  l1_cache[index][way].dirty = dirty;
  l1_cache[index][way].tag = tag;
  l1_cache[index][way].cycle = cycle + L1_LATENCY;
}

void write_l2_data(unsigned long address, int way, int dirty, unsigned long long cycle) {
  unsigned long tag, index, offset;

  tag = address >> 22;
  index = (address >> 6) & 0x7FFF;
  offset = address & 0x3F;

  if(way < 0) {
    way = get_least_recently_used(l2_cache[index], L1_WAYS);
  }

  l2_cache[index][way].valid = 1;
  l2_cache[index][way].dirty = dirty;
  l2_cache[index][way].tag = tag;
  l2_cache[index][way].cycle = cycle + L2_LATENCY;
}

int get_opcode(const char *filename, char *assembly, char *opcode, unsigned long *address, unsigned long *read_register1, unsigned long *read_register2, unsigned long *write_register){
  static FILE *file = NULL;
  char *sub_string = NULL;
  char *tmp_ptr = NULL;
  char buf[CHUNK];
  int i = 0, count = 0;

  if(file == NULL) {
    file = fopen(filename, "r");
    if(file == NULL) {
      printf("Could not open file.\n");
      exit(1);
    }
  }

  if(!fgets(buf, sizeof buf, file)) {
    return (0);
  }

  // Check trace file

  while(buf[i] != '\0') {
    count += (buf[i] == ';');
    i++;
  }

  if(count != 5) {
    printf("Error reading trace (Wrong  number of fields)\n");
    printf("%s", buf);
    exit(2);
  }

  // ASM
  sub_string = strtok_r(buf, ";", &tmp_ptr);
  strcpy(assembly, sub_string);

  // Address
  sub_string = strtok_r(NULL, ";", &tmp_ptr);
  *address = strtoul(sub_string,NULL,10);

  // Operation
  sub_string = strtok_r(NULL, ";", &tmp_ptr);
  strcpy(opcode, sub_string);

  // Read register 1
  sub_string = strtok_r(NULL, ";", &tmp_ptr);
  *read_register1 = strtoul(sub_string,NULL,10);

  // Read register 2
  sub_string = strtok_r(NULL, ";", &tmp_ptr);
  *read_register2 = strtoul(sub_string,NULL,10);

  // Write register
  sub_string = strtok_r(NULL, ";", &tmp_ptr);
  *write_register = strtoul(sub_string,NULL,10);

  return 1;
}

int main(int argc, char *const *argv) {
  char assembly[20];
  char opcode[20];
  int verbose = 0;
  int opt;
  unsigned int way;
  unsigned long address;
  unsigned long read_register1, read_register2, write_register;
  unsigned long l1_hit = 0, l1_miss = 0, l2_hit = 0, l2_miss = 0;
  unsigned long long cycles = 0;

  while((opt = getopt(argc, argv, "v")) != -1) {
    switch(opt) {
      case 'v':
        verbose = 1;
        break;
      default:
        fprintf(stderr, "Invalid option: -%c\n"
                        "Usage: %s [-v] <trace file>\n", optopt, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if(optind >= argc) {
    fprintf(stderr, "Usage: %s [-v] <trace file>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  while(get_opcode(argv[optind], assembly, opcode, &address, &read_register1, &read_register2, &write_register)) {
    ++cycles;

    if(verbose != 0) {
      printf(" Asm:%s", assembly);
      printf(" Opcode:%s", opcode);
      printf(" Address:%lu", address);
      printf(" First read register:%lu", read_register1);
      printf(" Second read register:%lu", read_register2);
      printf(" Write register:%lu", write_register);
      printf("\n");
    }

#ifndef CACHE_LOOKUP
#define CACHE_LOOKUP(address)   if(address != 0) {                                            \
                                  if(fetch_data_from_l1(address, &way) == FETCH_MISS) {       \
                                    if(fetch_data_from_l2(address, &way) == FETCH_MISS) {     \
                                      cycles += DRAM_LATENCY;                                 \
                                      ++l2_miss;                                              \
                                      write_l2_data(address, -1, 0, cycles);                  \
                                    } else {                                                  \
                                      ++l2_hit;                                               \
                                      write_l1_data(address, -1, 0, cycles);                  \
                                    }                                                         \
                                                                                              \
                                    cycles += L2_LATENCY;                                     \
                                    ++l1_miss;                                                \
                                  } else {                                                    \
                                    ++l1_hit;                                                 \
                                  }                                                           \
                                                                                              \
                                  cycles += L1_LATENCY;                                       \
                                }
#endif

    CACHE_LOOKUP(read_register1);
    CACHE_LOOKUP(read_register2);

    if(write_register != 0) {
      if(fetch_data_from_l1(write_register, &way) == FETCH_MISS) {
        if(fetch_data_from_l2(write_register, &way) == FETCH_MISS) {
          cycles += DRAM_LATENCY;
          ++l2_miss;
          write_l2_data(write_register, -1, 1, cycles);
        } else {
          ++l2_hit;
          write_l1_data(write_register, -1, 1, cycles);
        }

        cycles += L2_LATENCY;
        ++l1_miss;
      } else {
        ++l1_hit;
        write_l1_data(write_register, way, 1, cycles);
      }

      cycles += L1_LATENCY;
    }
  }

  fprintf(stdout, "Cycles: %llu\nL1 Hit/Miss: %lu/%lu\nL2 Hit/Miss: %lu/%lu\n", cycles, l1_hit, l1_miss, l2_hit, l2_miss);
  return 0;
}

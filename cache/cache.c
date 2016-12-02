#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Number of bytes to read at a time */
#define CHUNK                       1024

/* L1 cache parameters */
#define L1_SIZE                     (64 * 1024)
#define L1_WAYS                     4
#define L1_BLOCK_SIZE               64
#define L1_LATENCY                  2

/* L2 cache parameters */
#define L2_SIZE                     (2 * 1024 * 1024)
#define L2_WAYS                     8
#define L2_BLOCK_SIZE               64
#define L2_LATENCY                  4

/* Fetch return codes */
#define FETCH_HIT                   1
#define FETCH_MISS                  2

/* DRAM latency */
#define DRAM_LATENCY                150

/* PC based stride prefetcher table lines */
#define STRIDE_PREFETCHER_ENTRIES   64

/* Stride prefetcher states */
#define STATE_INIT                  0
#define STATE_TRANSIENT             1
#define STATE_STEADY                2
#define STATE_NO_PRED               3

/* Variable Length Delta Prefetcher parameters */
#define PAGE_SIZE                   (8 * 1024)
#define DELTA_HISTORY_LENGTH        64
#define DELTA_PREDICTION_TABLES     3
#define PREDICTION_TABLE_LENGTH     64

/* Invalid predictor (convention) */
#define INVALID_PREDICTOR           (9999)

/* Minimum function */
#define MIN(a,b)                    (((a) < (b)) ? (a) : (b))

/* Cache prefetcher */
#ifndef CACHE_PREFETCHER
#  define CACHE_PREFETCHER          variable_length_delta_prefetcher
#endif

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

struct reference_prediction_entry {
  unsigned long tag;
  unsigned long last_address;
  unsigned int stride;
  unsigned char state; /* Init - Trans - Steady - No Pred */
  int prefetch_used;
};

struct delta_history_table_entry {
  unsigned long page_number;
  unsigned long last_address;
  unsigned long cycle;
  unsigned int times_used;
  unsigned int last_predictor;
  unsigned int last_index;
  int last_deltas[4];
  int last_prefetched_offsets[4];
};

struct offset_prediction_table_entry {
  int delta_prediction;
  int accuracy;
  int first_access;
  unsigned long last_address;
};

struct delta_prediction_table_entry {
  int deltas[DELTA_PREDICTION_TABLES];
  int prediction;
  int accuracy;
  int nmru;
};

static struct cache_entry l1_cache[L1_SIZE / (L1_BLOCK_SIZE * L1_WAYS)][L1_WAYS];
static struct cache_entry l2_cache[L2_SIZE / (L2_BLOCK_SIZE * L2_WAYS)][L2_WAYS];
static unsigned long long total_prefetches = 0;
static unsigned long long useful_prefetches = 0;

int get_least_recently_used(struct cache_entry entries[], unsigned int nways) {
  int result = 0;
  unsigned long min_cycle;
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

int not_most_recently_used(struct delta_history_table_entry *dbh) {
  int result = 0, mru = 0;
  unsigned long max_cycle;
  unsigned int i;

  max_cycle = dbh[0].cycle;

  for(i = 1; i < DELTA_HISTORY_LENGTH; ++i) {
    if(dbh[i].cycle > max_cycle) {
      mru = i;
      max_cycle = dbh[i].cycle;
    }
  }

  while((result = rand() % DELTA_HISTORY_LENGTH) == mru);

  return result;
}

int fetch_data_from_l1(unsigned long address, unsigned int *way, unsigned long cycle, unsigned long *penalty) {
  unsigned long tag, index, offset __attribute((unused));
  unsigned int i;

  tag = address >> 14;
  index = (address >> 6) & 0xFF;
  offset = address & 0x3F;

  for(i = 0; i < L1_WAYS; ++i) {
    if(l1_cache[index][i].valid == 1 && l1_cache[index][i].tag == tag) {
      *way = i;
      *penalty = (l1_cache[index][i].cycle > cycle) ? (l1_cache[index][i].cycle - cycle) : 0;
      return FETCH_HIT;
    }
  }

  *penalty = 0;
  return FETCH_MISS;
}

int fetch_data_from_l2(unsigned long address, unsigned int *way, unsigned long cycle, unsigned long *penalty) {
  unsigned long tag, index, offset __attribute((unused));;
  unsigned int i;

  tag = address >> 19;
  index = (address >> 6) & 0xFFF;
  offset = address & 0x3F;

  for(i = 0; i < L2_WAYS; ++i) {
    if(l2_cache[index][i].valid == 1 && l2_cache[index][i].tag == tag) {
      *way = i;
      *penalty = (l2_cache[index][i].cycle > cycle) ? (l2_cache[index][i].cycle - cycle) : 0;
      return FETCH_HIT;
    }
  }

  *penalty = 0;
  return FETCH_MISS;
}

void write_l1_data(unsigned long address, int way, int dirty, unsigned long cycle) {
  unsigned long tag, index, offset __attribute__((unused));

  tag = address >> 14;
  index = (address >> 6) & 0xFF;
  offset = address & 0x3F;

  if(way < 0) {
    way = get_least_recently_used(l1_cache[index], L1_WAYS);
  }

  l1_cache[index][way].valid = 1;
  l1_cache[index][way].dirty = dirty;
  l1_cache[index][way].tag = tag;
  l1_cache[index][way].cycle = cycle + L1_LATENCY;
}

void write_l2_data(unsigned long address, int way, int dirty, unsigned long cycle) {
  unsigned long tag, index, offset __attribute__((unused));

  tag = address >> 19;
  index = (address >> 6) & 0xFFF;
  offset = address & 0x3F;

  if(way < 0) {
    way = get_least_recently_used(l2_cache[index], L1_WAYS);
  }

  l2_cache[index][way].valid = 1;
  l2_cache[index][way].dirty = dirty;
  l2_cache[index][way].tag = tag;
  l2_cache[index][way].cycle = cycle + L2_LATENCY;
}

void no_prefetcher(unsigned long pc, unsigned long address, unsigned long cycle, unsigned int missed_l2) {
  /* Does nothing */
}

void stride_based_prefetcher(unsigned long pc, unsigned long address, unsigned long cycle, unsigned int missed_l2) {
  static struct reference_prediction_entry reference_prediction_table[STRIDE_PREFETCHER_ENTRIES];
  static int initialized = 0;
  int index, available;
  unsigned int i;

  if(initialized == 0) {
    for(i = 0; i < STRIDE_PREFETCHER_ENTRIES; ++i) {
      reference_prediction_table[i].state = STATE_INIT;
    }

    initialized = 1;
  }

  index = -1;
  available = -1;

  for(i = 0; i < STRIDE_PREFETCHER_ENTRIES; ++i) {
    if(reference_prediction_table[i].state == STATE_INIT && available == -1) {
      available = i;
    }

    if(reference_prediction_table[i].tag == pc) {
      index = i;
      break;
    }
  }

  if(index == -1 && available != -1) {
    reference_prediction_table[available].tag = pc;
    reference_prediction_table[available].last_address = address;
    reference_prediction_table[available].state = STATE_TRANSIENT;
    reference_prediction_table[available].stride = 0;
  }

  if(index != -1) {
    /* Correct */
    if(reference_prediction_table[index].stride == address - reference_prediction_table[index].last_address) {
      if(reference_prediction_table[index].state == STATE_NO_PRED) {
        reference_prediction_table[index].state = STATE_TRANSIENT;
      } else {
        reference_prediction_table[index].state = STATE_STEADY;
      }

      if(reference_prediction_table[index].prefetch_used == 0) {
        ++useful_prefetches;
        reference_prediction_table[index].prefetch_used = 1;
      }
    /* Incorrect */
    } else {
      if(reference_prediction_table[index].state == STATE_INIT) {
        reference_prediction_table[index].stride = address - reference_prediction_table[index].last_address;
        reference_prediction_table[index].state = STATE_TRANSIENT;
      } else if(reference_prediction_table[index].state == STATE_TRANSIENT ||
                reference_prediction_table[index].state == STATE_NO_PRED) {
        reference_prediction_table[index].stride = address - reference_prediction_table[index].last_address;
        reference_prediction_table[index].state = STATE_NO_PRED;
      } else if(reference_prediction_table[index].state == STATE_STEADY) {
        reference_prediction_table[index].state = STATE_INIT;
      }
    }

    if(reference_prediction_table[index].state != STATE_NO_PRED) {
      reference_prediction_table[index].prefetch_used = 0;
      write_l2_data(address + reference_prediction_table[index].stride, -1, 0, cycle);
      ++total_prefetches;
    }

    reference_prediction_table[index].last_address = address;
  }
}

void variable_length_delta_prefetcher(unsigned long pc, unsigned long address, unsigned long cycle, unsigned int missed_l2) {
  static struct delta_history_table_entry delta_history_table[DELTA_HISTORY_LENGTH];
  static struct offset_prediction_table_entry offset_prediction_table[PAGE_SIZE / L2_BLOCK_SIZE];
  static struct delta_prediction_table_entry delta_prediction_table[DELTA_PREDICTION_TABLES][PREDICTION_TABLE_LENGTH];
  static int initialized = 0;
  unsigned int page_number, matches, last_predictor, last_index, i, j, k;
  int opt_index, dht_index = -1, dpt_index = -1, dpt_table = -1, delta = 0;

  if(initialized == 0) {
    for(i = 0; i < PAGE_SIZE / L2_BLOCK_SIZE; ++i) {
      offset_prediction_table[i].first_access = 0;
    }

    for(i = 0; i < DELTA_HISTORY_LENGTH; ++i) {
      delta_history_table[i].last_predictor = INVALID_PREDICTOR;
      delta_history_table[i].last_index = 0;
      delta_history_table[i].times_used = 0;
    }

    for(i = 0; i < DELTA_PREDICTION_TABLES; ++i) {
      for(j = 0; j < PREDICTION_TABLE_LENGTH; ++j) {
        delta_prediction_table[i][j].nmru = 1;
      }
    }

    initialized = 1;
  }

  /* Delta History Table */
  page_number = address / PAGE_SIZE;

  /* Fully associative search */
  for(i = 0; i < DELTA_HISTORY_LENGTH; ++i) {
    if(delta_history_table[i].page_number == page_number) {
      dht_index = i;
      break;
    }
  }

  if(missed_l2 == 0) {
    if(dht_index != -1) {
      for(i = 0; i < 4; ++i) {
        if(delta_history_table[dht_index].last_prefetched_offsets[i] == address) {
          ++useful_prefetches;
          goto pae; /* Ugly but works */
        }
      }
    }

    return; /* Not a PAE, don't do anything */
  }

pae:

  if(dht_index == -1) {
    dht_index = not_most_recently_used(delta_history_table);
    delta_history_table[dht_index].page_number = page_number;
    delta_history_table[dht_index].times_used = 0;
  } else {
    delta = (address % PAGE_SIZE) - delta_history_table[dht_index].last_address;
  }

  delta_history_table[dht_index].last_address = address % PAGE_SIZE;

  for(i = 0; i < 4; ++i) {
    delta_history_table[dht_index].last_deltas[i + 1] = delta_history_table[dht_index].last_deltas[i];
  }

  delta_history_table[dht_index].last_deltas[0] = delta;

  /* Offset Prediction Table */
  opt_index = (address % PAGE_SIZE) / L2_BLOCK_SIZE;

  if(offset_prediction_table[opt_index].first_access == 0) {
    offset_prediction_table[opt_index].delta_prediction = 0;
    offset_prediction_table[opt_index].accuracy = 0;
    offset_prediction_table[opt_index].first_access = 1;
  } else {
    if(offset_prediction_table[opt_index].accuracy == 1) {
      write_l2_data(address + offset_prediction_table[opt_index].delta_prediction, -1, 0, cycle);
      ++total_prefetches;
    }

    if(address - offset_prediction_table[opt_index].last_address == offset_prediction_table[opt_index].delta_prediction) {
      offset_prediction_table[opt_index].accuracy = 1;
    } else {
      if(offset_prediction_table[opt_index].accuracy == 0) {
        offset_prediction_table[opt_index].delta_prediction = address - offset_prediction_table[opt_index].last_address;
      }

      offset_prediction_table[opt_index].accuracy = 0;
    }
  }

  offset_prediction_table[opt_index].last_address = address;

  /* Delta Prediction Table */
  for(i = MIN(DELTA_PREDICTION_TABLES, delta_history_table[dht_index].times_used) - 1; i >= 0 && dpt_index == -1; --i) {
    for(j = 0; j < PREDICTION_TABLE_LENGTH && dpt_index == -1; ++j) {
      matches = 0;

      for(k = 0; k < i + 1; ++k) {
        if(delta_history_table[dht_index].last_deltas[k] == delta_prediction_table[i][j].deltas[k]) {
          ++matches;
        }
      }

      if(matches == i + 1) {
        dpt_table = i;
        dpt_index = j;
        break;
      }
    }
  }

  last_predictor = delta_history_table[dht_index].last_predictor;
  last_index = delta_history_table[dht_index].last_index;

  /* Update accuracy */
  if(last_predictor != INVALID_PREDICTOR) {
    if(delta_prediction_table[last_predictor][last_index].prediction == delta) {
      if(delta_prediction_table[last_predictor][last_index].accuracy < 3) {
        delta_prediction_table[last_predictor][last_index].accuracy++;
      }
    } else {
      if(delta_prediction_table[last_predictor][last_index].accuracy > 0) {
        delta_prediction_table[last_predictor][last_index].accuracy--;
      } else {
        delta_prediction_table[last_predictor][last_index].prediction = delta;
        delta_prediction_table[last_predictor][last_index].accuracy = 0;
      }
    }
  }

  /* Get new prediction */
  if(dpt_table != -1 && dpt_index != -1) {
    for(i = 0; i < 4; ++i) {
      delta_history_table[dht_index].last_prefetched_offsets[i + 1] = delta_history_table[dht_index].last_prefetched_offsets[i];
    }

    delta_history_table[dht_index].last_prefetched_offsets[0] = address + delta_prediction_table[dpt_table][dpt_index].prediction;
    delta_history_table[dht_index].last_predictor = dpt_table;
    delta_history_table[dht_index].last_index = dpt_index;
    write_l2_data(address + delta_prediction_table[dpt_table][dpt_index].prediction, -1, 0, cycle);
    ++total_prefetches;
  }

  /* New entry to Delta Prediction Table */
  if(delta_history_table[dht_index].times_used > 0) {
    dpt_table = MIN(DELTA_PREDICTION_TABLES, delta_history_table[dht_index].times_used) - 1;

    for(i = 0; i < PREDICTION_TABLE_LENGTH; ++i) {
      matches = 0;

      for(j = 0; j < dpt_table + 1; ++j) {
        if(delta_history_table[dht_index].last_deltas[j] == delta_prediction_table[dpt_table][i].deltas[j]) {
          ++matches;
        }
      }

      if(matches == dpt_table + 1) {
        goto entry_exists; /* Ugly but works! */
      }
    }

    do {
      dpt_index = rand() % PREDICTION_TABLE_LENGTH;
    } while(delta_prediction_table[dpt_table][dpt_index].nmru == 0);

    for(j = 0; j < PREDICTION_TABLE_LENGTH; ++j) {
      delta_prediction_table[dpt_table][j].nmru = 1;
    }

    for(j = 0; j < dpt_table + 1; ++j) {
      delta_prediction_table[dpt_table][dpt_index].deltas[j] = delta_history_table[dht_index].last_deltas[j];
    }

    delta_prediction_table[dpt_table][dpt_index].prediction = 0;
    delta_prediction_table[dpt_table][dpt_index].accuracy = 1;
  }

entry_exists:

  delta_prediction_table[dpt_table][dpt_index].nmru = 0;
  delta_history_table[dht_index].times_used++;
}

int get_opcode(const char *filename, char *assembly, char *opcode, unsigned long *address, unsigned long *read_register1,
               unsigned long *read_register2, unsigned long *write_register){
  static FILE *file = NULL;
  char *sub_string = NULL;
  char *tmp_ptr = NULL;
  char buf[CHUNK];
  int i = 0, count = 0;

  srand(time(NULL));

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
  double miss_rate, prefetch_rate;
  int verbose = 0;
  int opt;
  unsigned int way;
  unsigned long address;
  unsigned long read_register1, read_register2, write_register, missed_l2;
  unsigned long l1_hit = 0, l1_miss = 0, l2_hit = 0, l2_miss = 0;
  unsigned long cycles = 0, penalty = 0;

  while((opt = getopt(argc, argv, "v")) != -1) {
    switch(opt) {
      case 'v':
        verbose = 1;
        break;
      default:
        fprintf(stderr, "Usage: %s [-v] <trace file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if(optind >= argc) {
    fprintf(stderr, "Usage: %s [-v] <trace file>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  while(get_opcode(argv[optind], assembly, opcode, &address, &read_register1, &read_register2, &write_register)) {
    ++cycles;
    missed_l2 = 0;

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
#define CACHE_LOOKUP(mem)   if(mem != 0) {                                                          \
                              if(fetch_data_from_l1(mem, &way, cycles, &penalty) == FETCH_MISS) {   \
                                if(fetch_data_from_l2(mem, &way, cycles, &penalty) == FETCH_MISS) { \
                                  cycles += DRAM_LATENCY + penalty;                                 \
                                  ++l2_miss;                                                        \
                                  missed_l2 = 1;                                                    \
                                  write_l2_data(mem, -1, 0, cycles);                                \
                                } else {                                                            \
                                  ++l2_hit;                                                         \
                                  write_l1_data(mem, -1, 0, cycles);                                \
                                }                                                                   \
                                                                                                    \
                                cycles += L2_LATENCY + penalty;                                     \
                                ++l1_miss;                                                          \
                              } else {                                                              \
                                ++l1_hit;                                                           \
                              }                                                                     \
                                                                                                    \
                              cycles += L1_LATENCY + penalty;                                       \
                              CACHE_PREFETCHER(address, mem, cycles, missed_l2);                    \
                            }
#endif

    CACHE_LOOKUP(read_register1);
    CACHE_LOOKUP(read_register2);
    CACHE_LOOKUP(write_register);

/*
    if(write_register != 0) {
      if(fetch_data_from_l1(write_register, &way) == FETCH_MISS) {
        if(fetch_data_from_l2(write_register, &way) == FETCH_MISS) {
          cycles += DRAM_LATENCY; // Needed?
          ++l2_miss;              // Needed?
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
      CACHE_PREFETCHER(address, write_register, cycles);
    }
*/

  }

  miss_rate = (double) l1_miss + (double) l2_miss;
  miss_rate /= l1_miss + l2_miss + l1_hit + l2_hit;
  prefetch_rate = (total_prefetches > 0) ? ((double) useful_prefetches / (double) total_prefetches) : 0;
  fprintf(stdout, "Cycles: %lu\nL1 Hit/Miss: %lu/%lu\nL2 Hit/Miss: %lu/%lu\n", cycles, l1_hit, l1_miss, l2_hit, l2_miss);
  fprintf(stdout, "Prefetches Used/Total: %llu/%llu\n", useful_prefetches, total_prefetches);
  fprintf(stdout, "Miss Rate: %.6f\n", miss_rate);
  fprintf(stdout, "Prefetch Rate: %.6f\n", prefetch_rate);
  return 0;
}

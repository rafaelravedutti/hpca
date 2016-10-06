#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK                1024 /* read 1024 bytes at a time */
#define IPC                  1
#define BTB_SIZE             64
#define BTB_HIT              1 
#define BTB_MISS             5
#define BTB_MISS_PREDICTED   4
#define SIZE_HIST	     4
#define NUM_COUNTERS         16 /* 2**SIZE_HIST */
#define INCREASE             1
#define DECREASE             -1

typedef struct {
  unsigned long int address;
  unsigned long int target;
  int valid;
} branch_target_buffer;

typedef struct {
  int reg[SIZE_HIST];
} branch_history_register;


typedef struct {
  int counters[NUM_COUNTERS];
} pattern_history_table;

int get_opcode(char *assembly, char *opcode, unsigned long *address, unsigned long *size, unsigned *is_cond){
    char buf[CHUNK];
    size_t nread;
    char *sub_string = NULL;
    char *tmp_ptr = NULL;

    static FILE *file = NULL;

    if (file == NULL) {
        file = fopen("bzip2.txt", "r");
        if (file == NULL){
            printf("Could not open file.\n");
            exit(1);
        }
    }

    if(!fgets(buf, sizeof buf, file))
        return (0);

    // Check trace file
    int i=0, count=0;
    while (buf[i] != '\0')
    {
        count += (buf[i] == ';');
        i++;
    }
    if (count != 4)
    {
        printf("Error reading trace (Wrong  number of fields)\n");
        printf("%s", buf);
        exit(2);
    }

    //printf("%s\n", buf);

    // ASM
    sub_string = strtok_r(buf, ";", &tmp_ptr);
    strcpy(assembly, sub_string);

    // Operation
    sub_string = strtok_r(NULL, ";", &tmp_ptr);
    strcpy(opcode, sub_string);

    // Address
    sub_string = strtok_r(NULL, ";", &tmp_ptr);
    *address = strtoul(sub_string,NULL,10);

    // Size
    sub_string = strtok_r(NULL, ";", &tmp_ptr);
    *size = strtoul(sub_string,NULL,10);

    // Conditional
    sub_string = strtok_r(NULL, ";", &tmp_ptr);
    *is_cond = (sub_string[0] == 'C') ? 1: 0;

    return 1;
}

void insert_and_shift_bhr(branch_history_register *bhr, int val) {
  int i;

  for(i = 0; i < SIZE_HIST-1; i++)
    bhr->reg[i] = bhr->reg[i+1];

  bhr->reg[SIZE_HIST] = val;
}

void change_pht_counter(pattern_history_table *pht, int idx, int incr, branch_history_register *bhr) {
  int val;

  val = pht->counters[idx] + incr;
  
  if(val >= 0 && val < 4)
    pht->counters[idx] = val;
  
  if(incr == 1)  
    insert_and_shift_bhr(bhr, 1);
  else
    insert_and_shift_bhr(bhr, 0);
}

int main() {
  branch_target_buffer btb[BTB_SIZE];
  branch_history_register bhr;
  pattern_history_table pht;
  char assembly[20];
  char opcode[20];
  unsigned long address, next_address, next_fetch;
  unsigned long size, next_size;
  unsigned long long counter = 0;
  unsigned long acum_hit = 0, acum_miss = 0, acum_miss_pred = 0;
  unsigned int is_cond, next_is_cond; 
  unsigned int i, pht_idx, k;

  for(i = 0; i < BTB_SIZE; ++i) {
    btb[i].valid = 0;
  }

  for(i = 0; i < NUM_COUNTERS; i++) {
    pht.counters[i] = 0;
  } 

  for(i = 0; i < SIZE_HIST; i++) {
    bhr.reg[i] = 0;
  }

  size = 0;

  while(size != 0 || get_opcode(assembly, opcode, &address, &size, &is_cond)) { // already readen or read new instr
    next_size = 0;

    if(strncmp(opcode, "OP_BRANCH", 9) == 0) { // curr instr = branch instr
      i = address & 63; // 5 least significant bits

      if(btb[i].address == address && btb[i].valid != 0) { // found in BTB and valid
        if(get_opcode(assembly, opcode, &next_address, &next_size, &next_is_cond)) { // read next instr
          if(is_cond) { // conditional branch
            pht_idx = 0;
            for(k = 0; k < SIZE_HIST; k++) { //get pht index
              pht_idx += bhr.reg[k] << ((SIZE_HIST - 1) - k);
            }
            pht_idx = pht_idx ^ (address & 15);
            if(pht.counters[pht_idx] >= 2) { // get saturated counter
              next_fetch = btb[i].target;
            } else {
              next_fetch = address + size;
            }
            change_pht_counter(&pht, pht_idx,(next_address == address + size ? DECREASE : INCREASE), &bhr);
            if(next_fetch == next_address) {
              counter += BTB_HIT;
              acum_hit += BTB_HIT;
            } else {
              counter += BTB_MISS_PREDICTED;
              acum_miss_pred += BTB_MISS_PREDICTED;
              if(next_address != address + size)
	        btb[i].target = next_address;
            }
          } else {
            counter += BTB_HIT;
            acum_hit += BTB_HIT;
          }
        }
      } else {
        if(get_opcode(assembly, opcode, &next_address, &next_size, &next_is_cond)) { // read next instr
          if(next_address != address + size) {
            btb[i].address = address;
            btb[i].target = next_address;
            btb[i].valid = 1;
            counter += BTB_MISS;
            acum_miss += BTB_MISS;
          } else {
            counter += BTB_HIT;
            acum_hit += BTB_HIT;
          }
        }
      }
    } else {
      ++counter;
    }

    address = next_address;
    size = next_size;
    is_cond = next_is_cond;
  }

  printf("Cycles: %llu\nAcum_hit: %ld\nAcum_miss: %ld\nAcum_miss_pred: %ld\n", counter, acum_hit, acum_miss, acum_miss_pred);
}
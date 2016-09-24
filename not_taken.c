#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK                1024 /* read 1024 bytes at a time */
#define IPC                  1
#define BTB_SIZE             64
#define BTB_HIT              1 
#define BTB_MISS             5
#define BTB_MISS_PREDICTED   4

typedef struct {
  unsigned long int address;
  unsigned long int target;
  int valid;
} branch_table;

int get_opcode(const char *filename, char *assembly, char *opcode, unsigned long *address, unsigned long *size, unsigned *is_cond){
    char buf[CHUNK];
    size_t nread;
    char *sub_string = NULL;
    char *tmp_ptr = NULL;

    static FILE *file = NULL;

    if (file == NULL) {
        file = fopen(filename, "r");
        if (file == NULL){
            fprintf(stderr, "Could not open file.\n");
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
        fprintf(stderr, "Error reading trace (Wrong  number of fields)\n");
        fprintf(stderr, "%s", buf);
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

int main(int argc, const char *argv[]) {
  branch_table btb[BTB_SIZE];
  char assembly[20];
  char opcode[20];
  unsigned long address, next_address;
  unsigned long size, next_size;
  unsigned long long counter = 0;
  unsigned long acum_hit = 0, acum_miss = 0, acum_miss_pred = 0;
  unsigned int is_cond, next_is_cond; 
  unsigned int i;

  if(argc < 2) {
    fprintf(stdout, "Uso: %s <trace file>\n", argv[0]);
    exit(0);
  }

  for(i = 0; i < BTB_SIZE; ++i) {
    btb[i].address = 0;
    btb[i].valid = 0;
  }

  size = 0;

  while(size != 0 || get_opcode(argv[1], assembly, opcode, &address, &size, &is_cond)) {
    next_size = 0;

    if(strncmp(opcode, "OP_BRANCH", 9) == 0) {
      i = address & 63;

      if(btb[i].address == address && btb[i].valid != 0) {
        if(get_opcode(argv[1], assembly, opcode, &next_address, &next_size, &next_is_cond)) {
          if(is_cond) {
            if(address + size == next_address) {
              counter += BTB_HIT;
              acum_hit += BTB_HIT;
            } else {
              counter += BTB_MISS_PREDICTED;
              acum_miss_pred += BTB_MISS_PREDICTED;
            }
          } else {
            counter += BTB_HIT;
            acum_hit += BTB_HIT;
          }
        }
      } else {
        counter += BTB_MISS;
        acum_miss += BTB_MISS;
        btb[i].address = address;
        btb[i].valid = 1;
      }
    } else {
      ++counter;
    }

    address = next_address;
    size = next_size;
    is_cond = next_is_cond;
  }

  fprintf(stdout, "Cycles: %llu\nAcum_hit: %ld\nAcum_miss: %ld\nAcum_miss_pred: %ld\n", counter, acum_hit, acum_miss, acum_miss_pred);
  return 0;
}

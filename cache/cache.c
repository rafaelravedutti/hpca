#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 1024      /* read 1024 bytes at a time */

/* L1 cache parameters */
#define L1_SIZE         (64 * 1024)
#define L1_WAYS         4
#define L1_BLOCK_SIZE   64
#define L1_LATENCY      2

/* L2 cache parameters */
#define L2_SIZE         (2 * 1024 * 1024)
#define L2_WAYS         4
#define L2_BLOCK_SIZE   64
#define L2_LATENCY      4

/* Fetch return codes */
#define FETCH_HIT       1
#define FETCH_MISS      2

/* Policies:
   - Write Back with Write-Allocate
   - Last Recently Used (LRU) replacement
   - Non Inclusve
*/

static unsigned char l1_null_data[L1_BLOCK_SIZE] = { 0 };
static unsigned char l2_null_data[L2_BLOCK_SIZE] = { 0 };

int fetch_data_from_l1(unsigned long address, unsigned char data[L1_BLOCK_SIZE]) {
  return FETCH_MISS;
}

void write_l1_data(unsigned long address, unsigned char data[L1_BLOCK_SIZE]) {

}

int fetch_data_from_l2(unsigned long address, unsigned char data[L2_BLOCK_SIZE]) {
  return FETCH_MISS;
}

void write_l2_data(unsigned long address, unsigned char data[L2_BLOCK_SIZE]) {

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

int main(int argc, const char *argv[]) {
  char assembly[20];
  char opcode[20];
  unsigned long address;
  unsigned long read_register1, read_register2, write_register;

  while(get_opcode(argv[1], assembly, opcode, &address, &read_register1, &read_register2, &write_register)) {
    printf(" Asm:%s", assembly);
    printf(" Opcode:%s", opcode);
    printf(" Address:%lu", address);
    printf(" First read register:%lu", read_register1);
    printf(" Second read register:%lu", read_register2);
    printf(" Write register:%lu", write_register);
    printf("\n");
  }
}


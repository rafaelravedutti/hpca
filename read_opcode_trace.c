#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 1024 /* read 1024 bytes at a time */

int get_opcode(const char *filename, char *assembly, char *opcode, unsigned long *address, unsigned long *size, unsigned *is_cond){
    char buf[CHUNK];
    size_t nread;
    char *sub_string = NULL;
    char *tmp_ptr = NULL;

    static FILE *file = NULL;

    if (file == NULL) {
        file = fopen(filename, "r");
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

int main(int argc, const char *argv[]) {
    char assembly[20];
    char opcode[20];
    unsigned long address;
    unsigned long size;
    unsigned is_cond;

    while(get_opcode(argv[1], assembly, opcode, &address, &size, &is_cond)){
        printf(" Asm:%s", assembly);
        printf(" Opcode:%s", opcode);
        printf(" Address:%lu", address);
        printf(" Size:%lu", size);
        printf(" Cond?:%s", is_cond == 1? "C": "I");
        printf("\n");
    }
}


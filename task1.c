#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int main() {
    FILE *file;
    char filename[] = "output.txt";
    char write_str[] = "String from file";
    char buffer[100];
    long file_size;
    int i;
    
    
    file = fopen(filename, "w");
    if (file == NULL) {
        printf("ошибка создания файла.\n");
        return 1;
    }
    
    fprintf(file, "%s", write_str);
    fclose(file);
    
    printf("строка записана в: %s\n", write_str);
    
    
    file = fopen(filename, "r");
    if (file == NULL) {
        printf("ошибка открытия файла\n");
        return 1;
    }
    



    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    
    printf("чтение файла с конца:\n");
    
    
    
    for (i = file_size - 1; i >= 0; i--) {
        fseek(file, i, SEEK_SET);
        fread(&buffer[0], 1, 1, file);
        printf("%c", buffer[0]);
    }
    printf("\n");
    
    fclose(file);
    
    return 0;
}
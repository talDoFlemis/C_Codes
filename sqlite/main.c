#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct{
    char* buffer;
    size_t buffer_lenght;
    ssize_t input_lenght;
}InputBuffer;

InputBuffer* new_input_buffer(){
    InputBuffer* input_buffer =malloc(sizeof(InputBuffer));
    input_buffer->buffer=NULL;
    input_buffer->buffer_lenght=0;
    input_buffer->input_lenght=0;

    return input_buffer;
}

int read_input(InputBuffer* input_buffer){
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_lenght), stdin);

    if (bytes_read<=0)
    {       
        printf("Valor errado");
        exit(EXIT_FAILURE);
    }
    input_buffer->input_lenght = bytes_read -1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer){
    free(input_buffer->buffer);
    free(input_buffer);
}

void print_prompt(void){
    printf("db > ");
}

int main (int argc, char* argv[]){
    InputBuffer* input_buffer = new_input_buffer();

    while(true){
        print_prompt();
        read_input(input_buffer);

        if(strcmp(input_buffer->buffer, ".exit")==0){
            close_input_buffer(input_buffer);
            exit(EXIT_SUCCESS);
        }else{
            printf("O comando inputado '%s' não foi reconhecido\n", input_buffer->buffer);
        }
    }
}
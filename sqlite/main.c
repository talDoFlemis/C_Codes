#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

typedef enum{
    NODE_INTERNAL,
    NODE_LEAF
}NodeType;

typedef struct{
    char* buffer;
    size_t buffer_lenght;
    ssize_t input_lenght;
}InputBuffer;

typedef enum{
    META_SUCCESS,
    META_COMMAND_UNRECOGNIZED
}MetaCommandResult;

typedef enum{
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_NEGATIVE_ID,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_STRING_TOO_LONG
}PrepareResult;

typedef enum{
    STATEMENT_INSERT,
    STATEMENT_SELECT
}StatementType;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE +1];
    char email[COLUMN_EMAIL_SIZE+1];
}Row;

typedef struct{
    StatementType type;
    Row row_to_insert;
}Statement;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100

const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET+LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

typedef struct{
    int file_descriptor;
    uint32_t file_lenght;
    void* pages[TABLE_MAX_PAGES];
    uint32_t num_pages;
}Pager;

typedef struct {
    Pager* pager;
    uint32_t root_page_num;
} Table;

typedef struct{
    Table* table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
}Cursor;

typedef enum { 
    EXECUTE_SUCCESS, 
    EXECUTE_TABLE_FULL 
}ExecuteResult;

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
        printf("Valor errado\n");
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

Pager* pager_open (const char* filename){
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);

    if(fd==-1){
        printf("N??o foi poss??vel abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    off_t file_lenght = lseek(fd, 0, SEEK_END);
    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_lenght = file_lenght;
    pager->num_pages = (file_lenght / PAGE_SIZE);

    if(file_lenght%PAGE_SIZE != 0){
        printf("O banco de dados n??o possui quantidade de p??ginas inteiras, arquivo est?? corrompido\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i<TABLE_MAX_PAGES; i++){
        pager->pages[i] = NULL;
    }
    return pager;
}

void* get_page(Pager* pager, uint32_t page_num){
    if(page_num >TABLE_MAX_PAGES){
        printf("Tentando pegar o n??mero da pag??na fora dos limites. %d > %d\n", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }
    if(pager->pages[page_num] == NULL){
        void* page = malloc(sizeof(PAGE_SIZE));
        uint32_t num_pages = pager->file_lenght/PAGE_SIZE;
        
        if(pager->file_lenght%PAGE_SIZE){
            num_pages += 1;
        }
        if(page_num<=num_pages){
            lseek(pager->file_descriptor, page_num*PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);

            if(bytes_read==-1){
                printf("Erro ao tentar ler o arquivo %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        pager->pages[page_num] = page;

        if(page_num >=pager->num_pages){
            pager->num_pages = page_num +1;
        }
    }
    return pager->pages[page_num];
}

uint32_t* leaf_node_num_cells(void* node){
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* leaf_node_cell(void* node, uint32_t cell_num){
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num){
    return leaf_node_cell(node, cell_num);
}

void* leaf_node_value(void* node, uint32_t cell_num){
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void* node){
    *leaf_node_num_cells(node) = 0;
}

void serialize_row(Row* source, void* destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    strncpy(destination + USERNAME_OFFSET,source->username, USERNAME_SIZE);
    strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);

}

void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void print_constants() {
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
  printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
  printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
}

void print_leaf_node(void* node) {
  uint32_t num_cells = *leaf_node_num_cells(node);
  printf("leaf (size %d)\n", num_cells);
  for (uint32_t i = 0; i < num_cells; i++) {
    uint32_t key = *leaf_node_key(node, i);
    printf("  - %d : %d\n", i, key);
  }
}

void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value){
    void* node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t num_cells = *leaf_node_num_cells(node);
    if(num_cells >= LEAF_NODE_MAX_CELLS){
        printf("Ainda vai ser implementado a funcao de splitar o node \n ");
        exit(EXIT_FAILURE);
    }

    if(cursor->cell_num < num_cells){
        for (uint32_t i = num_cells; i > cursor->cell_num; i--){
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

Table* db_open(const char* filename){
    Pager* pager =  pager_open(filename);

    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if(pager->num_pages == 0 ){
        void* root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
    }

    return table;
}

void pager_flush(Pager* pager, uint32_t page_num){
    if(pager->pages[page_num] == NULL){
        printf("Tentando flushar uma p??gina nula\n");
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if(offset == -1){
        printf("Erro ao procurar %d\n", errno);
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_writen = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);

    if(bytes_writen == -1){
        printf("Erro ao tentar escrever no db%d\n", errno);
        exit(EXIT_FAILURE);
    }
}

void db_close(Table* table){
    Pager* pager = table->pager;

    for (uint32_t i = 0; i < pager->num_pages; i++)
    {
        if(pager->pages[i] == NULL){
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    int result = close(pager->file_descriptor);
    if(result == -1){
        printf("Erro ao tentar fechar o banco de dados\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i<TABLE_MAX_PAGES; i++){
        void* page = pager->pages[i];
        if(page){
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table){
    if(strcmp(input_buffer->buffer, ".exit")==0){
        db_close(table);
        exit(EXIT_SUCCESS);
    }else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_leaf_node(get_page(table->pager, 0));
        return META_SUCCESS;
    }else if (strcmp(input_buffer->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_SUCCESS;
    }else{
        return META_COMMAND_UNRECOGNIZED;
    }
}

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void* cursor_value(Cursor* cursor) {
    uint32_t page_num = cursor->page_num;
    void* page = get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor* cursor){
    uint32_t page_num = cursor->page_num;
    void* node = get_page(cursor->table->pager, page_num);

    cursor->cell_num += 1;
    if(cursor->cell_num >= (*leaf_node_num_cells(node))){
        cursor->end_of_table = true;
    }
}

Cursor* table_start(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    cursor->cell_num = 0;

    void* root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells==0);

    return cursor;
}

Cursor* table_end(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;

    void* root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->page_num = num_cells;
    cursor->end_of_table = true;

    return cursor;
}

ExecuteResult execute_insert(Statement* statement, Table* table) {
    void* node = get_page(table->pager, table->root_page_num);
        
    if ((*leaf_node_num_cells(node) >=  LEAF_NODE_MAX_CELLS)) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    Cursor* cursor = table_end(table);

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

    return EXECUTE_SUCCESS;
}

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement){
    statement->type = STATEMENT_INSERT;

    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " ");
    char* username_string = strtok(NULL, " ");
    char* email_string = strtok(NULL, " ");

    if(id_string == NULL ||username_string== NULL || email_string == NULL){
        return PREPARE_SYNTAX_ERROR;
    }
    
    int id = atoi(id_string);

    if(id < 0){
        return PREPARE_NEGATIVE_ID;
    }
    
    if(strlen(username_string) > COLUMN_USERNAME_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }
    if(strlen(email_string) > COLUMN_USERNAME_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username_string);
    strcpy(statement->row_to_insert.email, email_string);

    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){
    if(strncmp(input_buffer->buffer, "insert", 6)==0){
        return prepare_insert(input_buffer, statement);
    }
    if(strcmp(input_buffer->buffer, "select")==0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    Cursor* cursor = table_start(table);

    Row row;
    while (!(cursor->end_of_table))
    {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }
    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table) {

    switch (statement->type){
        case (STATEMENT_INSERT):
            return execute_insert(statement, table);
        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}


int main (int argc, char* argv[]){
    if(argc<2){
        printf("Por favor, voc?? deve fornecer um arquivo\n");
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    Table* table = db_open(filename);

    InputBuffer* input_buffer = new_input_buffer();

    while(true){
        print_prompt();
        read_input(input_buffer);

        if(input_buffer->buffer[0] == '.'){
            switch (do_meta_command(input_buffer, table))
            {
            case (META_SUCCESS):
                continue;
            case META_COMMAND_UNRECOGNIZED:
                printf("O comando '%s' n??o foi reconhecido\n", input_buffer->buffer);
                continue;
            }   
        }
        Statement statement;
        switch (prepare_statement(input_buffer, &statement)){
            case PREPARE_SUCCESS:
                break;
            case PREPARE_STRING_TOO_LONG:
                printf("A string ?? muito longa");
                continue;
            case PREPARE_NEGATIVE_ID:
                printf("O valor do id ?? negativo, por favor coloque n??meros positivos");
                continue;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
	            continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("A palavra chave '%s' n??o foi reconhecida\n", input_buffer->buffer);
                continue;
        }
        switch (execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
        }
    }
}
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>
#include <assert.h>
#include <string.h>

#define DEPTH_OF_FS 5

typedef struct NODE
{
    int data;
    struct NODE *next;
} node;

typedef struct LIST
{
    node *top;
    int size;
} list;

list *init_list()
{
    list *new_list = (list *)malloc(sizeof(list));

    new_list->size = 0;
    new_list->top = NULL;

    return new_list;
}

void enqueue(list *my_list, int data)
{

    node *newNode = NULL;

    // will allocate memory dynamically.
    newNode = malloc(sizeof(node));
    newNode->data = data;

    // if the table is empty already, the node will be added at the front
    if (my_list->top == NULL)
    {
        newNode->next = my_list->top;
        my_list->top = newNode;
    }
    else
    {
        node *current = my_list->top;
        node *previous = NULL;
        while (current->next != NULL)
        {
            previous = current;
            current = current->next;
        }

        if (current->next == NULL)
        {
            newNode->next = NULL;
            current->next = newNode;
        }
    }

    my_list->size++;
}

// deleting the element from the front and return it.
// return NULL if no element available
int dequeue(list *my_list)
{
    if (my_list->top != NULL)
    {

        int delete_data = 0;

        delete_data = my_list->top->data;

        free(my_list->top);

        if (my_list->top->next != NULL)
        {
            my_list->top = my_list->top->next;
        }
        else
        {
            my_list->top = NULL;
        }
        my_list->size--;
        return delete_data;
    }
    return -1;
}

void clear_list(list *my_list)
{
    assert(my_list != NULL);

    while (my_list->top != NULL)
    {
        dequeue(my_list);
    }
    free(my_list);
}

#pragma pack(push)
#pragma pack(1)

typedef struct INFO_EXFAT
{
    // some comman variable needed by the file reader
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t first_cluster_root;
    uint8_t value_for_sector_length;
    uint8_t value_for_sectors_per_cluster;
    uint8_t entry_type;
    uint32_t first_cluster_index;
    uint32_t next_index;

    // part -1 variables to print the specific info.
    uint32_t volume_serial_number;
    uint8_t label_length;
    uint16_t volume_label;
    uint32_t bitmap_cluster_index;
    uint64_t data_length_bitmap;
    uint8_t one_byte;

    // variables for part-2
    uint16_t attribute;
    uint16_t file_name;
    uint8_t file_name_length;
    uint64_t valid_data_length;

} info_exFAT;

#pragma pack(pop)

static char *unicode2ascii(uint16_t *unicode_string, uint8_t length)
{
    assert(unicode_string != NULL);
    assert(length > 0);

    char *ascii_string = NULL;

    if (unicode_string != NULL && length > 0)
    {
        // +1 for a NULL terminator
        ascii_string = calloc(sizeof(char), length + 1);

        if (ascii_string)
        {
            // strip the top 8 bits from every character in the
            // unicode string
            for (uint8_t i = 0; i < length; i++)
            {
                ascii_string[i] = (char)unicode_string[i];
            }
            // stick a null terminator at the end of the string.
            ascii_string[length] = '\0';
        }
    }

    return ascii_string;
}

int fat_offset_bytes = 0;
int cluster_heap_offset = 0;
int sector_length = 0;
int sectors_per_cluster = 0;
int first_cluster_index = 0;
int entry_type = 0;
int total_cluster = 0;
int first_cluster_root = 0;
int length_file_path = 0;
void common_variables_reader(int fd, info_exFAT *info_storage)
{
    lseek(fd, 80, SEEK_CUR);

    read(fd, &info_storage->fat_offset, 4);
    read(fd, &info_storage->fat_length, 4);
    read(fd, &info_storage->cluster_heap_offset, 4);
    read(fd, &info_storage->cluster_count, 4);
    read(fd, &info_storage->first_cluster_root, 4);
    read(fd, &info_storage->volume_serial_number, 4);

    lseek(fd, 4, SEEK_CUR);
    read(fd, &info_storage->value_for_sector_length, 1);
    read(fd, &info_storage->value_for_sectors_per_cluster, 1);

    sector_length = pow(2, info_storage->value_for_sector_length);
    sectors_per_cluster = pow(2, info_storage->value_for_sectors_per_cluster);
    fat_offset_bytes = (info_storage->fat_offset * sector_length);
    cluster_heap_offset = info_storage->cluster_heap_offset;
    total_cluster = info_storage->cluster_count;
    first_cluster_root = info_storage->first_cluster_root;
}

void print_info(int fd, info_exFAT *info_storage)
{

    list *root_list = init_list();

    info_storage->next_index = info_storage->first_cluster_root;

    while (info_storage->next_index != 4294967295)
    {
        enqueue(root_list, info_storage->next_index);
        lseek(fd, fat_offset_bytes + (info_storage->next_index * 4), SEEK_SET);
        read(fd, &info_storage->next_index, 4);
    }

    int N = (sectors_per_cluster * sector_length) / 32;

    node *temp = root_list->top;
    char *str = NULL;

    while (temp != NULL)
    {

        int i = 0;

        lseek(fd, (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length, SEEK_SET);

        while (i < N)
        {
            read(fd, &info_storage->entry_type, 1);
            if (info_storage->entry_type == 131)
            {
                read(fd, &info_storage->label_length, 1);
                read(fd, &info_storage->volume_label, 22);
                str = unicode2ascii(&info_storage->volume_label, info_storage->label_length);

                lseek(fd, 8, SEEK_CUR);
            }
            else if (info_storage->entry_type == 129)
            {
                lseek(fd, 19, SEEK_CUR);
                read(fd, &info_storage->bitmap_cluster_index, 4);

                read(fd, &info_storage->data_length_bitmap, 8);
            }
            else
            {
                lseek(fd, 31, SEEK_CUR);
            }
            i++;
        }

        temp = temp->next;
    }

    list *bitmap_clusters = init_list();
    info_storage->next_index = info_storage->bitmap_cluster_index;

    while (info_storage->next_index != 4294967295 && info_storage->next_index != 4294967287 && info_storage->next_index != 4294967288 && info_storage->next_index != 0)
    {
        enqueue(bitmap_clusters, info_storage->next_index);
        lseek(fd, fat_offset_bytes + (info_storage->next_index * 4), SEEK_SET);
        read(fd, &info_storage->next_index, 4);
    }

    temp = bitmap_clusters->top;

    int bit_count = 0;
    int zero_bits = 0;
    while (temp != NULL)
    {
        int count_bytes = 0;
        lseek(fd, (cluster_heap_offset + ((temp->data - 2) * sectors_per_cluster)) * sector_length, SEEK_SET);

        while (bit_count < total_cluster && count_bytes < (sector_length * sectors_per_cluster))
        {
            read(fd, &info_storage->one_byte, 1);
            count_bytes++;

            int bit_in_byte = 0;
            while (bit_in_byte < 8)
            {
                if (!(info_storage->one_byte & 1))
                    zero_bits++;
                info_storage->one_byte = info_storage->one_byte >> 1;

                bit_count++;
                bit_in_byte++;
            }
        }

        temp = temp->next;
    }

    printf("Info about exFAT file system...\n");
    printf("Volume Label : %s\n", str);
    printf("Volume Serial Number : %d\n", info_storage->volume_serial_number);
    printf("Free space in the file system (in KB) : %d KB\n", (zero_bits * sector_length * sectors_per_cluster) / 1024);
    printf("Cluster Size (In sectors) : %d sectors\n", sectors_per_cluster);
    printf("Cluster Size (In bytes) : %d bytes\n", sector_length * sectors_per_cluster);

    clear_list(bitmap_clusters);
    clear_list(root_list);
}

void print_file_list(int start_index, int file_descriptor, info_exFAT *info, char *s, char *arg, char *str[DEPTH_OF_FS], int index)
{
    info->first_cluster_index = start_index;

    list *list_1 = init_list();
    while (info->first_cluster_index != 4294967295 && info->first_cluster_index != 4294967287 && info->first_cluster_index != 0)
    {
        enqueue(list_1, info->first_cluster_index);
        lseek(file_descriptor, fat_offset_bytes + (info->first_cluster_index * 4), SEEK_SET);
        read(file_descriptor, &info->first_cluster_index, 4);
    }

    int value_N = (sectors_per_cluster * sector_length) / 32;

    node *temp = list_1->top;

    while (temp != NULL)
    {

        int i = 0;
        int offset = (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length;
        lseek(file_descriptor, offset, SEEK_SET);

        while (i < value_N)
        {

            lseek(file_descriptor, offset + i * 32, SEEK_SET);
            read(file_descriptor, &info->entry_type, 1);

            if (info->entry_type == 133)
            {
                lseek(file_descriptor, offset + i * 32 + 4, SEEK_SET);
                read(file_descriptor, &info->attribute, 2);
                info->attribute = info->attribute >> 4;

                i++;

                lseek(file_descriptor, offset + i * 32, SEEK_SET);
                read(file_descriptor, &info->entry_type, 1);

                if (info->entry_type == 192)
                {

                    lseek(file_descriptor, offset + i * 32 + 3, SEEK_SET);
                    read(file_descriptor, &info->file_name_length, 1);

                    int length = info->file_name_length;

                    lseek(file_descriptor, offset + i * 32 + 8, SEEK_SET);
                    read(file_descriptor, &info->valid_data_length, 8);

                    int valid_data_length = info->valid_data_length;

                    lseek(file_descriptor, offset + i * 32 + 20, SEEK_SET);
                    read(file_descriptor, &info->first_cluster_index, 4);
                    i++;
                    lseek(file_descriptor, offset + i * 32, SEEK_SET);
                    read(file_descriptor, &info->entry_type, 1);

                    if (info->entry_type == 193)
                    {

                        char *file = malloc(256);
                        while (info->entry_type == 193)
                        {

                            lseek(file_descriptor, offset + i * 32 + 2, SEEK_SET);
                            read(file_descriptor, &info->file_name, 30);
                            i++;

                            char *temp_filename = unicode2ascii(&info->file_name, length);
                            strcat(file, temp_filename);

                            lseek(file_descriptor, offset + i * 32, SEEK_SET);
                            read(file_descriptor, &info->entry_type, 1);
                        }

                        if (strcmp(arg, "list") == 0)
                        {
                            if (info->attribute & 1)
                            {

                                printf("%s Directory :  %s\n", s, file);
                                char *newstr = malloc(sizeof(s) + 1);
                                newstr[0] = '-';
                                newstr[1] = '\0';
                                strcat(newstr, s);
                                print_file_list(info->first_cluster_index, file_descriptor, info, newstr, arg, str, index);
                                free(newstr);
                            }
                            else
                            {
                                printf("%s File :  %s\n", s, file);
                            }
                        }
                        else if (strcmp(arg, "get") == 0)
                        {
                            if (index < length_file_path && strcmp(file, str[index]) == 0)
                            {
                                if (index + 1 < length_file_path)
                                    print_file_list(info->first_cluster_index, file_descriptor, info, NULL, arg, str, index + 1);
                                else
                                {
                                    int fs_file_d = open(file, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);

                                    list *file_data_list = init_list();

                                    while (info->first_cluster_index != 4294967295 && info->first_cluster_index != 4294967287 && info->first_cluster_index != 0)
                                    {
                                        enqueue(file_data_list, info->first_cluster_index);
                                        lseek(file_descriptor, fat_offset_bytes + (info->first_cluster_index * 4), SEEK_SET);
                                        read(file_descriptor, &info->first_cluster_index, 4);
                                    }

                                    node *temp_2 = file_data_list->top;

                                    while (temp_2 != NULL)
                                    {

                                        int j = 0;
                                        int offset = (cluster_heap_offset + (temp_2->data - 2) * sectors_per_cluster) * sector_length;
                                        lseek(file_descriptor, offset, SEEK_SET);

                                        while (j < value_N && j < valid_data_length)
                                        {
                                            read(file_descriptor, &info->one_byte, 1);
                                            printf("xxxx :%c\n", info->one_byte);
                                            write(fs_file_d, &info->one_byte, 1);
                                            j++;
                                        }

                                        temp_2 = temp_2->next;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
                i++;
        }

        temp = temp->next;
    }
}
int main(int argc, char *argv[])
{
    assert(argc >= 3);

    int fd = open(argv[1], O_RDONLY);
    info_exFAT info_struct;

    common_variables_reader(fd, &info_struct);

    if (strcmp(argv[2], "info") == 0)
    {

        print_info(fd, &info_struct);
    }
    else
    {

        char *token = strtok(argv[3], "/");
        char *file_path[DEPTH_OF_FS];

        while (token != NULL)
        {
            file_path[length_file_path] = malloc(strlen(token));
            strcpy(file_path[length_file_path], token);
            token = strtok(NULL, "/");
            printf("file name : %s\n", file_path[length_file_path]);
            fflush(stdout);
            length_file_path++;
        }

        print_file_list(first_cluster_root, fd, &info_struct, "", argv[2], file_path, 0);
    }

    return 0;
}

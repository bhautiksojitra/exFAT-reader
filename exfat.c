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

    uint8_t label_length;
    uint16_t volume_label;

    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    int cluster_count;
    uint32_t first_cluster_root;
    uint32_t volume_serial_number;

    uint8_t value_for_sector_length;
    uint8_t value_for_sectors_per_cluster;

    uint8_t entry_type;
    uint8_t entry_type_2;
    uint32_t data;
    long data_2;

    uint32_t first_cluster_index;
    uint64_t data_length_bitmap;

    uint8_t one_byte;

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

int main(int argc, char *argv[])
{
    assert(argc >= 3);

    int fd = open(argv[1], O_RDONLY);

    if (strcmp(argv[2], "info") == 0)
    {

        info_exFAT *info_storage = malloc(sizeof(info_exFAT));
        double sector_length = 0;
        double sectors_per_cluster = 0;
        list *new_list = init_list();

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

        info_storage->data = info_storage->first_cluster_root;

        printf("\nfirst_cluster_root : %d", info_storage->first_cluster_root);

        int fat_offset_bytes = (info_storage->fat_offset * sector_length);
        int cluster_offset_bytes = info_storage->cluster_heap_offset;
        int cluster_size = info_storage->cluster_count;

        printf("fat offset bytes : %d\n", fat_offset_bytes);

        while (info_storage->data != 4294967295)
        {
            enqueue(new_list, info_storage->data);
            printf("\n data : 0x%08x", info_storage->data);
            lseek(fd, fat_offset_bytes + (info_storage->data * 4), SEEK_SET);
            read(fd, &info_storage->data, 4);
        }

        int N = (new_list->size * sectors_per_cluster * sector_length) / 32;

        printf("\ncluster_offset_bytes : %d", cluster_offset_bytes);

        printf("\nfat_offset : %d", info_storage->fat_offset);
        printf("\nfat_length : %d", info_storage->fat_length);
        printf("\ncluster_heap_offset : %d", info_storage->cluster_heap_offset);
        printf("\ncluster_count : %d", info_storage->cluster_count);

        printf("\nvolume serial number : %d", info_storage->volume_serial_number);
        printf("\nsector_length :%f\n", sector_length);
        printf("\nsectors_per_cluster : %f\n", sectors_per_cluster);

        node *temp = new_list->top;

        while (temp != NULL)
        {

            int i = 0;
            lseek(fd, (cluster_offset_bytes + (temp->data - 2) * sectors_per_cluster) * sector_length, SEEK_SET);

            while (i < N && (info_storage->label_length <= 0 || info_storage->data_length_bitmap <= 0))
            {
                read(fd, &info_storage->entry_type, 1);
                if (info_storage->entry_type == 131)
                {
                    read(fd, &info_storage->label_length, 1);
                    read(fd, &info_storage->volume_label, 22);
                    lseek(fd, 8, SEEK_CUR);
                }
                else if (info_storage->entry_type == 129)
                {
                    lseek(fd, 19, SEEK_CUR);
                    read(fd, &info_storage->first_cluster_index, 4);
                    read(fd, &info_storage->data_length_bitmap, 8);
                }
                else
                {
                    lseek(fd, 31, SEEK_CUR);
                }
                i++;
                printf("\n0x%02x", info_storage->entry_type);
            }

            temp = temp->next;
        }

        char *str = unicode2ascii(&info_storage->volume_label, info_storage->label_length);

        printf("volume label : %s", str);

        printf("value of N : %d", N);

        printf("\nfat_offset : %d", info_storage->fat_offset);
        printf("\nfat_length : %d", info_storage->fat_length);
        printf("\ncluster_heap_offset : %d", info_storage->cluster_heap_offset);
        printf("\ncluster_count : %d", info_storage->cluster_count);

        printf("\nvolume serial number : %d", info_storage->volume_serial_number);
        printf("\nsector_length :%f\n", sector_length);
        printf("\nsectors_per_cluster : %f\n", sectors_per_cluster);

        // allocation bit map
        printf("\nfirst cluster index : %d", info_storage->first_cluster_index);
        printf("\ndata length bitmap : %lu", info_storage->data_length_bitmap);

        list *cluster_list = init_list();
        info_storage->data_2 = info_storage->first_cluster_index;

        printf("fat offset bytes : %d\n", fat_offset_bytes);

        while (info_storage->data_2 != 4294967295)
        {
            enqueue(cluster_list, info_storage->data_2);

            lseek(fd, fat_offset_bytes + (info_storage->data_2 * 4), SEEK_SET);
            read(fd, &info_storage->data_2, 4);
            printf("\n data 2 : 0x%08lx", info_storage->data_2);
        }

        node *temp_2 = cluster_list->top;
        printf("\ncluster_offset_bytes : %d", cluster_offset_bytes);

        int bit_count = 0;
        int set_bit = 0;
        while (temp_2 != NULL)
        {
            int x = 0;

            x = (cluster_offset_bytes + ((temp_2->data - 2) * sectors_per_cluster)) * sector_length;

            printf("\nvalue of x :: ----------- %d", x);

            int count = 0;

            printf("temp data : %d\n", temp_2->data);

            lseek(fd, (cluster_offset_bytes + ((temp_2->data - 2) * sectors_per_cluster)) * sector_length, SEEK_SET);

            while (bit_count < cluster_size && count < sector_length * sectors_per_cluster)
            {
                read(fd, &info_storage->one_byte, 1);

                printf("\none byte   : %d", info_storage->one_byte);

                count++;

                //printf("\nCount : %d", count);

                int n = 0;
                while (n < 8)
                {
                    if (!(info_storage->one_byte & 1))
                        set_bit++;
                    printf("%d", info_storage->one_byte && 1);
                    info_storage->one_byte = info_storage->one_byte >> 1;
                    bit_count++;
                    n++;
                }
            }

            temp_2 = temp_2->next;
        }

        long y = (info_storage->cluster_count - set_bit);
        printf(" set bit : \n %d", set_bit);
        printf("\ncluster size : %d  %d", info_storage->cluster_count, bit_count);
        printf("\ntotal free space : %ld", (y * 512 * 4) / 1000);

        clear_list(cluster_list);
        clear_list(new_list);
    }
    else if (strcmp(argv[2], "list") == 0)
    {
    }
    else if (strcmp(argv[2], "get") == 0)
    {
    }

    return 0;
}

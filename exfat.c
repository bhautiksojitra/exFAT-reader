//-----------------------------------------
// NAME: Bhautik Sojitra
// STUDENT NUMBER: 7900140
// COURSE: COMP 3430, SECTION: A01
// INSTRUCTOR: Franklin Bristow
// ASSIGNMENT: 4
//
// REMARKS: exFAT file system reader
//
//-----------------------------------------

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

#define DEPTH_OF_FS 100
#define INDEX_ZERO 0

// node structure
typedef struct NODE
{
    int data;
    struct NODE *next;
} node;

// list as a queue
typedef struct LIST
{
    node *top;
    int size;
} list;

#pragma pack(push)
#pragma pack(1)

// structure that stores the variables to read the volume of file system.
typedef struct INFO_EXFAT
{
    // some comman variable needed by the file reader
    uint32_t fat_offset;                   // the starting address of the FAT section
    uint32_t fat_length;                   //length of the FAT entries.
    uint32_t cluster_heap_offset;          // the starting address of the cluster heap.
    uint32_t cluster_count;                // total clusters in the volume.
    uint32_t first_cluster_root;           // index of the first cluster of the root directory
    uint8_t value_for_sector_length;       // the value given in the boot sector for the sector length in bytes
    uint8_t value_for_sectors_per_cluster; // the value in the boot sector stating the size of the cluster in sectors
    uint8_t entry_type;                    // stores the entry type of the directory entries ( 1st data)

    // to create the cluster chain
    uint32_t first_cluster_index; // index of the first cluster for any directory
    uint32_t next_index;          // next index in the cluster chain

    // part -1 variables to print the specific info.
    uint32_t volume_serial_number; // number that we get from the boot sector
    uint8_t label_length;          // length of the volume label
    uint16_t volume_label;         // the unicode string of the volume label
    uint32_t bitmap_cluster_index; // first cluster index of the allocation bitmap
    uint64_t data_length_bitmap;   // length of the bitmap
    uint8_t one_byte;              // just to read one byte whenever needed

    // variables for part-2
    uint16_t attribute;         // file attribute (to differentiate directory and file)
    uint16_t file_name;         //name of the file
    uint8_t file_name_length;   // length of the file name
    uint64_t valid_data_length; // data length stored in any file or directory.

} info_exFAT;

#pragma pack(pop)

//------------------------PROTOTYPES ---------------------------------------------------------------

// prototypes for queue data structure.

list *init_list();
void enqueue(list *, int);
int dequeue(list *);
void clear_list(list *);

// prototyes for file system reader

static char *unicode2ascii(uint16_t *unicode_string, uint8_t length);
void common_variables_reader(int fd, info_exFAT *info_storage);
list *build_cluster_chain(int start_index, info_exFAT *info, int fd);
void print_info(int fd, info_exFAT *info_storage);
void get_user_data(char *file_name, int file_descriptor, int start_index, long data_length, info_exFAT *info);
void recursive_file_finder(int start_index, int file_descriptor, info_exFAT *info, char *s, char *arg, char *str[DEPTH_OF_FS], int index_str_arr);

// ----------------------------------------QUEUE IMPLEMENTATION-------------------------------------------

// Initialise all the variables of the queue
list *init_list()
{
    // dynamic memory allocation to the queue
    list *new_list = (list *)malloc(sizeof(list));

    new_list->size = 0;
    new_list->top = NULL;

    return new_list;
}

// Inserting the data in the queue (here the data is an integer)
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

// cleaning up the list and freeing the memory
void clear_list(list *my_list)
{
    assert(my_list != NULL);

    while (my_list->top != NULL)
    {
        dequeue(my_list);
    }
    free(my_list);
}

//---------------------------------------- QUEUE ENDS ------------------------------------------------

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

// global variables to use the variables in every function of the program
int fat_offset_bytes = 0;
int cluster_heap_offset = 0;
int sector_length = 0;
int sectors_per_cluster = 0;
int first_cluster_index = 0;
int entry_type = 0;
int total_cluster = 0;
int first_cluster_root = 0;
int length_file_path = 0;
int file_found = 0;

// reads the common data from the file system that is needed everywhere
// MUST be called before calling other functions.
void common_variables_reader(int fd, info_exFAT *info_storage)
{

    // reading specific data and storing it in the global variables.
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

// to buld the cluster chain for any directory in the volume
list *build_cluster_chain(int start_index, info_exFAT *info, int fd)
{
    list *new_list = init_list();

    info->next_index = start_index;

    // loop through the fat indexes and fnd out what is the list of cluster the directory holds
    while (info->next_index != 4294967295 && info->next_index != 4294967287 && info->next_index != 4294967288 && info->next_index != 0)
    {
        enqueue(new_list, info->next_index); // adding index to the list.
        lseek(fd, fat_offset_bytes + (info->next_index * 4), SEEK_SET);
        read(fd, &info->next_index, 4);
    }

    return new_list;
}

//----------------------------------------- INFO ---------------------------------------------------
// printing info for the INFO COMMAND execution
void print_info(int fd, info_exFAT *info_storage)
{

    list *root_list = build_cluster_chain(info_storage->first_cluster_root, info_storage, fd);

    int dir_entries_in_cluster = (sectors_per_cluster * sector_length) / 32;

    // loop through the directories in the cluster chain
    node *temp = root_list->top;
    char *str = NULL;

    while (temp != NULL)
    {

        int i = 0;

        // go to the cluster of the first index.
        lseek(fd, (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length, SEEK_SET);

        // examine all the entries in the cluster
        while (i < dir_entries_in_cluster)
        {
            // reading the entry type
            read(fd, &info_storage->entry_type, 1);

            // 131 means volume label directory (that holds the label of the volume )
            if (info_storage->entry_type == 131)
            {
                // reads the appropriate data (unicode string and string length)
                read(fd, &info_storage->label_length, 1);
                read(fd, &info_storage->volume_label, 22);
                // convert unicode string to ascii string
                str = unicode2ascii(&info_storage->volume_label, info_storage->label_length);

                lseek(fd, 8, SEEK_CUR);
            }
            else if (info_storage->entry_type == 129) // 129 means allocation bitmap directory
            {
                // reads the index and build the cluster chain for the bitmap
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

    // the cluster chain for the allocation bitmap
    list *bitmap_clusters = build_cluster_chain(info_storage->bitmap_cluster_index, info_storage, fd);

    temp = bitmap_clusters->top;

    int bit_count = 0; // total bits in the bitmap
    int zero_bits = 0; // zero bits means number of free clusters

    while (temp != NULL)
    {

        int count_bytes = 0; // count the byte that you read in one iteration

        // go to the allocation bitmap first cluster
        lseek(fd, (cluster_heap_offset + ((temp->data - 2) * sectors_per_cluster)) * sector_length, SEEK_SET);

        // checks till the cluster ends or allocation bitmap ends
        while (bit_count < total_cluster && count_bytes < (sector_length * sectors_per_cluster))
        {
            // read one byte only
            read(fd, &info_storage->one_byte, 1);
            count_bytes++;

            int bit_in_byte = 0; // bits in a byte is 8
            while (bit_in_byte < 8 && bit_count < total_cluster)
            {
                if (!(info_storage->one_byte & 1)) // if a bit is 0 then add it indicating as a free cluster
                    zero_bits++;
                info_storage->one_byte = info_storage->one_byte >> 1; // shift to the next bit in a byte

                bit_count++;
                bit_in_byte++;
            }
        }

        temp = temp->next;
    }

    // printing all the required info
    printf("Info about exFAT file system...\n");
    printf("Volume Label : %s\n", str);
    printf("Volume Serial Number : %d\n", info_storage->volume_serial_number);
    printf("Free space in the file system (in KB) : %d KB\n", (zero_bits * sector_length * sectors_per_cluster) / 1024);
    printf("Cluster Size (In sectors) : %d sectors\n", sectors_per_cluster);
    printf("Cluster Size (In bytes) : %d bytes\n", sector_length * sectors_per_cluster);

    // free the memory
    clear_list(bitmap_clusters);
    clear_list(root_list);
}

//--------------------------------------------- INFO DONE ----------------------------------------

void get_user_data(char *file_name, int file_descriptor, int start_index, long data_length, info_exFAT *info)
{

    // creating the a file
    int fs_fd = open(file_name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IROTH);

    printf("File %s created successfully !\n ", file_name);

    // build cluster chain for the file
    list *file_data_list = build_cluster_chain(start_index, info, file_descriptor);

    node *temp_2 = file_data_list->top;

    // reads the data from the clusters of the file
    long track_data_length = 0;
    int cluster_in_bytes = (sectors_per_cluster * sector_length);

    printf("Writing Data in the file.... \n");

    while (temp_2 != NULL)
    {

        int j = 0;

        int file_cluster_offset = (cluster_heap_offset + (temp_2->data - 2) * sectors_per_cluster) * sector_length;
        lseek(file_descriptor, file_cluster_offset, SEEK_SET);

        // reads each byte from the cluster and writes it to the file
        // reads until the cluster ends or the no more data left
        while (j < cluster_in_bytes && track_data_length < data_length)
        {
            read(file_descriptor, &info->one_byte, 1);
            write(fs_fd, &info->one_byte, 1);

            j++;
            track_data_length++;
        }

        temp_2 = temp_2->next; // go to the next cluster.
    }

    printf("The data has been written !");
    clear_list(file_data_list);
}

// finds the meta data about the  desired file using recursion
void recursive_file_finder(int start_index, int file_descriptor, info_exFAT *info, char *s, char *arg, char *str[DEPTH_OF_FS], int index)
{

    // building the cluster chain for the directory that we are working on.
    list *list_1 = build_cluster_chain(start_index, info, file_descriptor);

    int dir_entries_per_cluster = (sectors_per_cluster * sector_length) / 32;

    node *temp = list_1->top;

    // loop through the cluster chain
    while (temp != NULL)
    {

        // track the directory entry index
        int i = 0;
        int offset = (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length;

        // checks each dir entry in the cluster
        while (i < dir_entries_per_cluster)
        {

            lseek(file_descriptor, offset + i * 32, SEEK_SET);
            read(file_descriptor, &info->entry_type, 1);

            if (info->entry_type == 133) // 133 = x85 - file directory
            {
                lseek(file_descriptor, offset + i * 32 + 4, SEEK_SET);
                read(file_descriptor, &info->attribute, 2);
                info->attribute = info->attribute >> 4;

                i++;

                // check condition if the file directory spans across multiple clusters
                // if x85 is last dir entry in the cluster , then
                // to check xc0 it will go to another cluster in the chain and reset the track variables
                if (i == dir_entries_per_cluster && temp->next != NULL)
                {
                    temp = temp->next;
                    i = 0;
                    offset = (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length;
                }

                lseek(file_descriptor, offset + i * 32, SEEK_SET);
                read(file_descriptor, &info->entry_type, 1);

                if (info->entry_type == 192)
                {

                    //reads appropriate data and store them to the local var
                    // use of local var is because we don't lose the original value.
                    // In case of struct var is used anywhere else.
                    lseek(file_descriptor, offset + i * 32 + 3, SEEK_SET);
                    read(file_descriptor, &info->file_name_length, 1);

                    int length = info->file_name_length;

                    lseek(file_descriptor, offset + i * 32 + 8, SEEK_SET);
                    read(file_descriptor, &info->valid_data_length, 8);
                    long valid_data_length = info->valid_data_length;

                    lseek(file_descriptor, offset + i * 32 + 20, SEEK_SET);
                    read(file_descriptor, &info->first_cluster_index, 4);
                    i++;

                    // check condition if the file directory spans across multiple clusters
                    // if xc0 is last dir entry in the cluster , then
                    // to check xc1, it will go to another cluster in the chain and reset the track variables
                    if (i == dir_entries_per_cluster && temp->next != NULL)
                    {
                        temp = temp->next;
                        i = 0;
                        offset = (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length;
                    }

                    lseek(file_descriptor, offset + i * 32, SEEK_SET);
                    read(file_descriptor, &info->entry_type, 1);

                    if (info->entry_type == 193)
                    {

                        char *file = malloc(256);
                        // reads the file name which is spreaded in multiple directory entries.
                        while (info->entry_type == 193)
                        {

                            // read a part of the file name
                            lseek(file_descriptor, offset + i * 32 + 2, SEEK_SET);
                            read(file_descriptor, &info->file_name, 30);
                            i++;

                            if (i == dir_entries_per_cluster && temp->next != NULL)
                            {
                                temp = temp->next;
                                i = 0;
                                offset = (cluster_heap_offset + (temp->data - 2) * sectors_per_cluster) * sector_length;
                            }

                            // convert the string and concat it to get the full name in one var.
                            char *temp_filename = unicode2ascii(&info->file_name, length);
                            strcat(file, temp_filename);

                            lseek(file_descriptor, offset + i * 32, SEEK_SET);
                            read(file_descriptor, &info->entry_type, 1);
                        }

                        // this part is when list command is passed
                        if (strcmp(arg, "list") == 0)
                        {
                            // if directory then
                            if (info->attribute & 1)
                            {

                                // prints the name of the directory
                                printf("%s Directory :  %s\n", s, file);

                                // Indicates the level of depth in the volume for the directory.
                                char *newstr = malloc(sizeof(s) + 1);
                                newstr[0] = '-';
                                newstr[1] = '\0';
                                strcat(newstr, s);

                                // recursively go into the directory to find other files and folders in it.
                                // Using the index of first cluster.
                                recursive_file_finder(info->first_cluster_index, file_descriptor, info, newstr, arg, str, index);
                                free(newstr);
                            }
                            else
                            {
                                // If file then just print the name of the file.
                                printf("%s File :  %s\n", s, file);
                            }
                        }
                        else if (strcmp(arg, "get") == 0)
                        {
                            // recursive call only if it finds the file passed in the argument
                            if (index < length_file_path && strcmp(file, str[index]) == 0)
                            {

                                // condition indicates more directories to check from the path
                                // to get to the actual file
                                if (index + 1 < length_file_path)
                                {
                                    printf("Directory found : %s\n", file);
                                    recursive_file_finder(info->first_cluster_index, file_descriptor, info, "", arg, str, index + 1);
                                }
                                else
                                {
                                    // gets the data entered by the user from the retreived file.
                                    printf("File found : %s\n", file);
                                    file_found = 1;
                                    get_user_data(file, file_descriptor, info->first_cluster_index, valid_data_length, info);
                                }
                            }
                        }
                    }
                }
            }
            else
                i++; // go to the next dir entry.
        }

        temp = temp->next; // go to the next cluster
    }

    clear_list(list_1); // free the memory of the list.
}

// Main method - use to call functions of the program
// Simulator of the file system reader
int main(int argc, char *argv[])
{
    // the arguments should be atleast 3.
    assert(argc >= 3);

    // opening the file from the argument for the reading
    int fd = open(argv[1], O_RDONLY);
    info_exFAT info_struct;

    //reading common variables - general purpose use
    common_variables_reader(fd, &info_struct);

    // calling functions based on the command passed in as a argument
    if (strcmp(argv[2], "info") == 0)
    {
        print_info(fd, &info_struct);
    }
    else if (strcmp(argv[2], "list") == 0)
    {

        // will print the list of all files in the volume recursively
        assert(first_cluster_root > 0); // error checking to move forward.
        recursive_file_finder(first_cluster_root, fd, &info_struct, "", argv[2], NULL, INDEX_ZERO);
    }
    else if (strcmp(argv[2], "get") == 0)
    {
        // if path is found then execute the get command
        if (argv[3] != NULL)
        {
            // tokenise the arg[3] string and store ot in an array of strings.
            char *token = strtok(argv[3], "/");
            char *file_path[DEPTH_OF_FS];

            while (token != NULL)
            {
                file_path[length_file_path] = token;

                token = strtok(NULL, "/");
                length_file_path++; // length of the path to track how many directories we need to check.
            }

            // finds the path of the using the 6th para-meter and creates the file in our system.
            assert(first_cluster_root > 0); // error checking to move forward.
            recursive_file_finder(first_cluster_root, fd, &info_struct, NULL, argv[2], file_path, INDEX_ZERO);

            // Error msg if no file is found for get command.
            if (!file_found)
                printf("Couldn't find the file through the given path !\n");
        }
        else
        {
            printf("Couldn't find the path of the file ! pass it in 4th argument... \n");
        }
    }
    else
    {
        printf("Invalid command entered ! check argv[2]... \n");
    }

    printf("End of processing...\n ");
    return 0;
}

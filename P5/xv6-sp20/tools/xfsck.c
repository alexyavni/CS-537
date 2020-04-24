// Compile with:
// gcc -iquote ../include -Wall -Werror -ggdb -o xfsck xfsck.c
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <string.h>

#define stat xv6_stat     // avoid clash with host struct stat
#define dirent xv6_dirent // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat
#undef dirent

void print_inode(struct dinode dip);

int main(int argc, char *argv[])
{
    int fd;
    if (argc == 2)
    {
        // If image file exists - open in read only
        fd = open(argv[1], O_RDONLY);
    }
    else
    {
        fprintf(stderr, "Usage: program fs.img\n");
        exit(1);
    }

    if (fd < 0)
    {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }
    // File opens correctly

    struct stat sbuf;
    fstat(fd, &sbuf);

    // if you want to access a file's contents
    void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    struct superblock *sb = (struct superblock *)(img_ptr + BSIZE);

    // ******************************* CHECK 1 *******************************
    // For the metadata in the super block, the file-system size is larger than the number of blocks used
    // by the super-block, inodes, bitmaps and data. If not, print err msg
    int total = 4 + (sb->ninodes / IPB) + sb->nblocks;
    if (total > sb->size)
    {
        fprintf(stderr, "ERROR: superblock is corrupted.\n");
        exit(1);
    }

    // Root directory: /
    // inode 0 is unused again
    // inode 1 belongs to the root directory
    char *bitmap = (char *)(img_ptr + BBLOCK(0, sb->ninodes) * BSIZE);

    struct dinode *dip = (struct dinode *)(img_ptr + 2 * BSIZE);
    uint data_block_addr = dip[1].addrs[0];
    uint size_fs = sb->size;
    uint curr_block_addr;

    int j = 1;
    int dot_found = 0;
    int two_dot_found = 0;

    int list_addrs_used[sb->nblocks];
    int list_addrs_used_i[sb->ninodes];
    int in_use_inodes[sb->ninodes];

    for (int i = 0; i < sb->nblocks; i++)
    {
        list_addrs_used[i] = 0;
        if(i < sb->ninodes)
        {
            list_addrs_used_i[i] = 0;
            in_use_inodes[i] = 0;
        }
    }

    while (j < sb->ninodes)
    {
        // check if type is 0 - if so , continue (skip that inode)
        short type = dip[j].type;
        if (type == 0)
        {
            j++;
            continue;
        }

        // ******************************* CHECK 2 *******************************
        // Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV)
        if (type != 0 && type != T_DEV && type != T_DIR && type != T_FILE)
        {
            fprintf(stderr, "ERROR: bad inode.\n");
            exit(1);
        }

        in_use_inodes[j] = 1;
        uint num_blocks = 0;

        // ******************************* CHECK 3 *******************************
        // printf("MAX = %d, MIN = %d, j = %d\n", size_fs, data_block_addr, j);
        for (int k = 0; k < NDIRECT; k++)
        {
            if (dip[j].addrs[k] != 0 && (dip[j].addrs[k] >= size_fs || dip[j].addrs[k] < data_block_addr))
            {
                fprintf(stderr, "ERROR: bad direct address in inode.\n");
                exit(1);
            }

            // ******************************* CHECK 5 *******************************
            // For in-use inodes, each address in use is also marked in use in the bitmap
            char bit_l = bitmap[(dip[j].addrs[k]) / 8];
            int mask = 0x1 << ((dip[j].addrs[k]) % 8);
            char bit_masked = bit_l & mask;
            int bit_final = bit_masked >> ((dip[j].addrs[k]) % 8);
            bit_final = bit_final & 1;
            if (bit_final != 1)
            {
                fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                exit(1);
            }

            if (dip[j].addrs[k] != 0)
            {
                num_blocks++;
                if (list_addrs_used[dip[j].addrs[k]] == 1)
                {
                    fprintf(stderr, "ERROR: direct address used more than once.\n");
                    exit(1);
                }
                list_addrs_used[dip[j].addrs[k]] = 1;
            }
        }

        // Indirect addresses block ptr
        uint *indirect = (uint *)(img_ptr + BSIZE * dip[j].addrs[NDIRECT]);

        if (dip[j].addrs[NDIRECT] != 0)
        {
            list_addrs_used[dip[j].addrs[NDIRECT]] = 1;
            for (int i = 0; i < BSIZE / sizeof(uint); ++i)
            {
                if (indirect[i] != 0 && (indirect[i] >= size_fs || indirect[i] < data_block_addr))
                {
                    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                    exit(1);
                }

                // ******************************* CHECK 5 *******************************
                // For in-use inodes, each INDIRECT address in use is also marked in use in the bitmap
                char bit_l = bitmap[indirect[i] / 8];
                int mask = 0x1 << (indirect[i] % 8);
                char bit_masked = bit_l & mask;
                int bit_final = bit_masked >> (indirect[i] % 8);
                bit_final = bit_final & 1;
                if (bit_final != 1)
                {
                    fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                    exit(1);
                }

                if (indirect[i] != 0)
                {
                    num_blocks++;
                    list_addrs_used[indirect[i]] = 1;
                }
            }
        }

        if (dip[j].type == T_FILE)
        {
            uint max = num_blocks * 512;
            uint min = (num_blocks - 1) * 512 + 1;
            if(dip[j].size != 0 && (dip[j].size > max || dip[j].size < min))
            {
                fprintf(stderr, "ERROR: incorrect file size in inode.\n");
                exit(1);
            }

        }

        if (dip[j].type == T_DIR)
        {
            dot_found = 0;
            two_dot_found = 0;
            curr_block_addr = dip[j].addrs[0];
            struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + curr_block_addr * BSIZE);
            int num_entries = (dip[j].size / (DIRSIZ + 2));
            for (int i = 0; i < num_entries; ++i)
            {
                if (strcmp(entry[i].name, ".") == 0)
                {
                    // . directory
                    // Make sure it points to self
                    dot_found = 1;
                    if (entry[i].inum != j)
                    {
                        fprintf(stderr, "ERROR: directory not properly formatted.\n");
                        exit(1);
                    }
                }
                else if (strcmp(entry[i].name, "..") == 0)
                {
                    // .. directory
                    two_dot_found = 1;
                }
                else
                {
                    // Add directory entry inum to inode reference list
                    list_addrs_used_i[entry[i].inum] = 1;
                }
            }

            // ******************************* CHECK 4 *******************************
            // Each directory contains . and .. entries, and the . entry points to the directory itself.
            if (two_dot_found == 0 || dot_found == 0)
            {
                fprintf(stderr, "ERROR: directory not properly formatted.\n");
                exit(1);
            }
        }

        j++;
    }

    // How to parse root directory contents?
    // dip[1] is the root inode and addrs[0] is the first block within that
    // struct xv6_dirent *dent = (struct xv6_dirent*) (img_ptr + dip[1].addrs[0] * BSIZE);

    // printf("Number of inodes in one block %ld\n", IPB);
    // unused       | superblock | inode blocks [25] | unused    | bitmap (data)    | data blocks [995]
    // 0            | 1          | 2 ... 26          | 27        | 28               | 29 -> data blocks
    // 1 + 1 + 25 + 2 + 995 = 1024 total blocks

    // ******************************* CHECK 6 *******************************
    // For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect
    // block somewhere.
    char bit_l;
    int mask;
    char bit_masked;
    int bit_final;

    for (int i = 2; i < sb->ninodes; i++)
    {
        if (in_use_inodes[i] == 1 && list_addrs_used_i[i] == 0)
        {
            fprintf(stderr, "ERROR: inode marked used but not found in a directory.\n");
            exit(1);
        }
        else if (in_use_inodes[i] == 0 && list_addrs_used_i[i] == 1)
        {
            fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
            exit(1);
        }
    }

    for (int i = data_block_addr; i < sb->nblocks; i++)
    {
        bit_l = bitmap[i / 8];
        mask = 0x1 << (i % 8);
        bit_masked = bit_l & mask;
        bit_final = bit_masked >> (i % 8);
        bit_final = bit_final & 1;

        if (bit_final == 1 && list_addrs_used[i] == 0)
        {
            fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
            exit(1);
        }
    }

    return 0;
}

void print_inode(struct dinode dip)
{
    printf("file type:%d,", dip.type);
    printf("nlink:%d,", dip.nlink);
    printf("size:%d,", dip.size);
    printf("first_addr:%d\n", dip.addrs[0]);
}
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

#define stat xv6_stat  // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat
#undef dirent

void print_inode(struct dinode dip);

int main(int argc, char *argv[]) 
{
    int fd;
    if(argc == 2)
    {
        // If image fiel exists - open in read only
        fd = open(argv[1], O_RDONLY);
    }
    else
    {
        fprintf(stderr, "Usage: program fs.img\n");
        exit(1);
    }

    if(fd < 0)
    {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }
    // File opens correctly

    struct stat sbuf;
    fstat(fd, &sbuf);
    // printf("Image %ld in size\n", sbuf.st_size);

    // if you want to access a file's contents
    //      (a) Use read/write/fread/fwrite API - stream oriented method
    //      (b) Use mmap
    void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // If I want to read the first byte of the file
    // img_ptr[0];

    // Block 0 is unused
    // Block 1 is superblock
    // Inodes start at block 2
    // It is interpreting bytes from byte 512 as struct superblock
    // WARNING: Be careful with the type cast
    struct superblock *sb = (struct superblock *) (img_ptr + BSIZE);
    // printf("size %d nblocks %d ninodes %d\n", sb->size, sb->nblocks, sb->ninodes);
    
    // ******************************* CHECK 1 *******************************
    // For the metadata in the super block, the file-system size is larger than the number of blocks used 
    // by the super-block, inodes, bitmaps and data. If not, print err msg
    int total = 4 + (sb->ninodes / IPB) + sb->nblocks;
    // printf("total = %d, file sys size = %d\n", total, sb->size);
    if(total > sb->size)
    {
        fprintf(stderr, "ERROR: superblock is corrupted.\n");
        exit(1);
    }

    // Root directory: /
    // inode 0 is unused again
    // inode 1 belongs to the root directory
    // Inode has 12 direct data blocks and 1 indirect data block
    //
    // unused | superblock | inode blocks
    // root inode probably in block 2
    struct dinode *dip = (struct dinode *) (img_ptr + 2 * BSIZE);
    // print_inode(dip[0]);
    // print_inode(dip[1]);

    uint data_block_addr = dip[1].addrs[0];
    uint size_fs = sb->size;
    uint curr_block_addr;

    int j = 1;
    int dot_found = 0;
    int two_dot_found = 0;
    // printf("max = %d, min = %d\n", size_fs, data_block_addr);
    while(j < sb->ninodes)
    {
        // check if type is 0 - if so , continue (skip that inode)
        short type = dip[j].type;
        if(type == 0)
        {
            j++;
            continue;
        }
        // ******************************* CHECK 2 *******************************
        // Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV)
        if(type != 0 && type != T_DEV && type != T_DIR && type != T_FILE)
        {
            fprintf( stderr, "ERROR: bad inode.\n");
            exit(1);
        }

        // iterate NDIRECT addresses for each inode
        // id addr 0 - we don't check this one
        // last one points to list of indirect addrs
        // if last entry is non-zero - there is indirect block (if valid)
        //      iterate indirect addresses - div by sizeof ptr to iterate the block
        // printf("addr = %d, size = %d, FILE TYPE = %d.\n", dip[j].addrs[0], dip[j].size, type);

        // ******************************* CHECK 3 *******************************

        // printf("MAX = %d, MIN = %d, j = %d\n", size_fs, data_block_addr, j);
        for(int k = 0; k < NDIRECT; k++)
        {
            if(dip[j].addrs[k] != 0 && (dip[j].addrs[k] >= size_fs || dip[j].addrs[k] < data_block_addr))
            {
                // printf("ERROR on addr = %d\n", dip[j].addrs[k]);
                fprintf(stderr, "ERROR: bad direct address in inode.\n");
                exit(1);
            }
        }
        // Indirect addresses block ptr
        uint* indirect = (uint*) (img_ptr + BSIZE * dip[j].addrs[NDIRECT]);

        if(dip[j].addrs[NDIRECT] != 0)
        {
            for (int i = 0; i < BSIZE/sizeof(uint); ++i) {
                // printf("indir addr #%d %d\n", i, indirect[i]);
                if(indirect[i] != 0 && (indirect[i] >= size_fs || indirect[i] < data_block_addr))
                {
                    // printf("ERROR on indir addr = %d\n", indirect[i]);
                    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                    exit(1);
                }
            }  
        }

        if(dip[j].type == T_DIR)
        {
            // ******************************* CHECK 4 *******************************
            // Each directory contains . and .. entries, and the . entry points to the directory itself. 
            dot_found = 0;
            two_dot_found = 0;
            curr_block_addr = dip[j].addrs[0];
            struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + curr_block_addr * BSIZE);
            for (int i = 0; i < DIRSIZ; ++i) {
                // printf("name is %s inum is %d\n", entry[i].name, entry[i].inum);
                if(strcmp(entry[i].name,".") == 0)
                {
                    // . directory
                    // Make sure it points to self
                    // TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                    dot_found = 1;
                    // printf("Dot dir with inum = %d, j = %d\n", entry[i].inum, j);
                    if(entry[i].inum != j)
                    {
                        fprintf( stderr, "ERROR: directory not properly formatted.\n");
                        exit(1);
                    }
                }
                else if(strcmp(entry[i].name,"..") == 0)
                {
                    // .. directory
                    two_dot_found = 1;
                }
            }

            if(two_dot_found == 0 || dot_found == 0)
            {
                fprintf( stderr, "ERROR: directory not properly formatted.\n");
                exit(1);
            }
        }
        
        j++;

        // Check 4: Each directory contains . and .. entries, 
        // and the . entry points to the directory itself. 
        // (NOTE: The root directory / should also have both entries). 
        // If not, print ERROR: directory not properly formatted.
        // cmp dir pointing to self - use inum
    }

    // How to parse root directory contents?
    // dip[1] is the root inode and addrs[0] is the first block within that
    struct xv6_dirent *dent = (struct xv6_dirent*) (img_ptr + dip[1].addrs[0] * BSIZE);
    // int i = 0;
    while(dent->inum != 0)
    {
        // Walking down the directory tree
        // printf("dir ent %d has name %s and inum %d\n", i++, dent->name, dent->inum);
        dent++;
    }
    // printf("Number of inodes in one block %ld\n", IPB);
    // unused       | superblock | inode blocks [25] | unused    | bitmap (data)    | data blocks [995]
    // 0            | 1          | 2 ... 26          | 27        | 28               | 29 -> data blocks 
    // 1 + 1 + 25 + 2 + 995 = 1024
    //
    // --------- Total blocks 1024


    // struct superblock {
    //   uint size;         // Size of file system image (blocks)
    //   uint nblocks;      // Number of data blocks
    //   uint ninodes;      // Number of inodes.
    // };

    // struct dinode {
    // short type;           // File type
    // short major;          // Major device number (T_DEV only)
    // short minor;          // Minor device number (T_DEV only)
    // short nlink;          // Number of links to inode in file system
    // uint size;            // Size of file (bytes)
    // uint addrs[NDIRECT+1];   // Data block addresses
    // };

    // struct dirent {
    // ushort inum;
    // char name[DIRSIZ];
    // };

    // Access bitmap - make sure you consider BITS vs bytes
    // With 512 bytes you can store 4096 bits - so we have enough bits for our data blocks

    return 0;
}

void print_inode(struct dinode dip) {
  printf("file type:%d,", dip.type);
  printf("nlink:%d,", dip.nlink);
  printf("size:%d,", dip.size);
  printf("first_addr:%d\n", dip.addrs[0]);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



/*
 *   ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ ___ 
 *  |   |   |   |   |                       |   |
 *  | 0 | 1 | 2 | 3 |     .....             |127|
 *  |___|___|___|___|_______________________|___|
 *  |   \    <-----  data blocks ------>
 *  |     \
 *  |       \
 *  |         \
 *  |           \
 *  |             \
 *  |               \
 *  |                 \
 *  |                   \
 *  |                     \
 *  |                       \
 *  |                         \
 *  |                           \
 *  |                             \
 *  |                               \
 *  |                                 \
 *  |                                   \
 *  |                                     \
 *  |                                       \
 *  |                                         \
 *  |                                           \
 *  |     <--- super block --->                   \
 *  |______________________________________________|
 *  |               |      |      |        |       |
 *  |        free   |      |      |        |       |
 *  |       block   |inode0|inode1|   .... |inode15|
 *  |        list   |      |      |        |       |
 *  |_______________|______|______|________|_______|
 *
 *
 */


#define FILENAME_MAXLEN 8  // including the NULL char
#define BLOCK_SIZE 1024
char *FILE_CONTENT = "abcdefghijklmnopqrstuvwxyz";

/* 
 * inode 
 */

typedef struct inode {
  int  dir;  // boolean value. 1 if it's a directory.
  char name[FILENAME_MAXLEN];
  int  size;  // actual file/directory size in bytes.
  int  blockptrs [8];  // direct pointers to blocks containing file's content.
  int  used;  // boolean value. 1 if the entry is in use.
  int  rsvd;  // reserved for future use
} inode;


/* 
 * directory entry
 */

typedef struct dirent {
  char name[FILENAME_MAXLEN];
  int  namelen;  // length of entry name
  int  inode;  // this entry inode index
} dirent;

//datablk struct with either file or dir
typedef struct datablock{
  char file[BLOCK_SIZE];
  struct dirent directory[64];

} datablock;

//superblk struct with freeblk list and inode table
typedef struct superblock{
   int  freeBlkList[127];
   struct inode inodeTable[16];

} superblock;

//disk struct with a superblk and datablk
typedef struct disk{
  struct superblock mySuperblk;
  struct datablock myDatablk[127];
} disk;

disk myDisk;
FILE *myfs;



void update_state(){
  myfs = fopen("./myfs", "w");
  fwrite(&myDisk, sizeof(disk), 1, myfs);
  fclose(myfs);
}

int read_state(){
  myfs = fopen("./myfs", "r");
  int val = fread(&myDisk, sizeof(disk), 1, myfs);
  fclose(myfs);
  return val;
}

//initializes the disk with root directory
int initialize(){
  if ((myfs = fopen("./myfs", "r"))){
    // reads the state of the disk from myfs.
    fclose(myfs);
    read_state();
    return 2;
  }
  //root directory initialization of SB
  myDisk.mySuperblk.freeBlkList[0] = 1; //first datablk is occupied
  myDisk.mySuperblk.inodeTable[0].dir = 1; //root is a dir so true
  strcpy(myDisk.mySuperblk.inodeTable[0].name, "root");
  myDisk.mySuperblk.inodeTable[0].used = 1; //inode 0 is used for rootdir
  myDisk.mySuperblk.inodeTable[0].size = 0; //initially the size of the dir is 0
  myDisk.mySuperblk.inodeTable[0].blockptrs[0] = 0; //datablk 0 has rootdir
  myDisk.mySuperblk.inodeTable[0].rsvd = 0;

  //root directory initialization of DB
  for(int i=0; i<64; i++){
    strcpy(myDisk.myDatablk[0].directory[i].name, "");
    myDisk.myDatablk[0].directory[i].inode = -1; //indicates has no subdir/file
    myDisk.myDatablk[0].directory[i].namelen = 0;
  }

  //initialize unused space
  for(int i=1; i<127; i++){
    myDisk.mySuperblk.freeBlkList[i] = 0; //indicates that blks are free
  }
  for(int i=1; i<16; i++){
    myDisk.mySuperblk.inodeTable[i].dir = -1; //indicates there is no dir/file
    strcpy(myDisk.mySuperblk.inodeTable[i].name, "");
    myDisk.mySuperblk.inodeTable[i].size = 0;
    myDisk.mySuperblk.inodeTable[i].used = 0;
    myDisk.mySuperblk.inodeTable[i].rsvd = 0;
    for (int j = 0; j<8; j++){
      myDisk.mySuperblk.inodeTable[i].blockptrs[j] = -1;
    }
  }
  // updates the state of the disk in myfs
  update_state();
  return 0;
}

//returns the inode of the parent directory
int getParentInode(char *path){
  char *parent = strtok(path, "/");
  char *child = parent;
  char *tmp = child;
  char * next = "";
  while(1){
    int check = 0;
    parent = child;
    if(strcmp(next, "")==0){
      tmp = strtok(NULL, "/");
    }else{
    tmp = next;
    }
    child = tmp;
    next = strtok(NULL, "/");
    //find inode of parent
    for(int i =1; i< 16; i++){
        if(strcmp(myDisk.mySuperblk.inodeTable[i].name, parent) == 0){
          check ++;
          if(next == NULL){
            //return the inode of the parent
            return i;
          }
          break;
        }
    }
    if(check == 0){
      printf("the directory %s in the given path does not exist\n", parent);
      return -1; 
    }
  }
  return -1;
}

// returns the datablk of the parent dir
int getParentBlk(char *path){
  int inode = getParentInode(path);
  if (inode == -1){
    return -1;
  }
  else return myDisk.mySuperblk.inodeTable[inode].blockptrs[0];
}

//returns free inode from the inodeTable
int getFreeInode(){
  int freeInode = 1;
  while(myDisk.mySuperblk.inodeTable[freeInode].used != 0 && freeInode < 16){
    freeInode ++;
  }
  if(freeInode >=16){
    printf("no free inode \n");
    return -1;
  }
  return freeInode;
}

//returns freeblk from the freeblk lst
int getFreeBlock(){
    int freeblk = 1; 
    while(myDisk.mySuperblk.freeBlkList[freeblk] != 0 && freeblk < 127){
      freeblk++;
    }
    if(freeblk > 127){
      freeblk = -1;
    }
    return freeblk;
}

//update the inode if new dir created
void updateDirInode(int freeblk, int freeInode, char *name){
  myDisk.mySuperblk.inodeTable[freeInode].used = 1;
  myDisk.mySuperblk.inodeTable[freeInode].dir = 1;
  strcpy(myDisk.mySuperblk.inodeTable[freeInode].name , name);  
  //adding an inode of the new directory in the inode list.
  myDisk.mySuperblk.inodeTable[freeInode].blockptrs[0] = freeblk;
  myDisk.mySuperblk.inodeTable[freeInode].size= 0;
  myDisk.mySuperblk.freeBlkList[freeblk] = 1;
}

// creates a new directory
int CD(char *dirname){
  // reads the state of the disk from myfs.
  read_state();
  char dirPathcopy[100];
  strcpy(dirPathcopy, dirname);
  char *child = strtok(dirPathcopy, "/");
  char *temp = child;
  int count = 0;
  //find child
  while(temp != NULL){
      child = temp;
      count ++;
      temp = strtok(NULL, "/");
  }
  int parentBlk;
  if (count == 1){
    parentBlk = 0;
  }
  else{
      strcpy(dirPathcopy, dirname);
      //get parent's datablk
      parentBlk = getParentBlk(dirPathcopy);
      if (parentBlk == -1){
        return -1;
      }
  }
  // check if file already exists
  for (int i = 0; i < 64; i++){
    if (strcmp(myDisk.myDatablk[parentBlk].directory[i].name,child)==0){
      printf("the directory already exists\n");
      return -1;
    }
  }
  //get a free inode
  int freeInode = getFreeInode();
  if (freeInode == -1){
    return -1;
  }
  //get a free blk
  int freeblock = getFreeBlock();
  if (freeblock == -1){
    printf("no space in datablock\n");
    return -1;
  }
  //find free index in the dirent list of the parent dir
  int freeDirIdx = 0;
  while(strcmp(myDisk.myDatablk[parentBlk].directory[freeDirIdx].name, "") != 0){
    freeDirIdx++;
  }
  // initiating an empty directory list of the new directory
  for(int j = 0; j < 64; j++){    
    strcpy(myDisk.myDatablk[freeblock].directory[j].name,"");
    myDisk.myDatablk[freeblock].directory[j].namelen = 0;
    myDisk.myDatablk[freeblock].directory[j].inode = -1;
  }
  // creating dirent entry for the new dir in the parent dir
  strcpy(myDisk.myDatablk[parentBlk].directory[freeDirIdx].name, child);  
  myDisk.myDatablk[parentBlk].directory[freeDirIdx].namelen = strlen(child);
  myDisk.myDatablk[parentBlk].directory[freeDirIdx].inode = freeInode;
  //updating the freeinode for created dir
  updateDirInode(freeblock, freeInode, child);
  int parentInode;
  if (parentBlk == 0){ //parent is root
    parentInode = 0;
  }
  //find parent inode and update the size 
  else parentInode = getParentInode(dirname);
  myDisk.mySuperblk.inodeTable[parentInode].size += sizeof(dirent);
  // updates the state of the disk in myfs
  update_state();
  return 0;  
}

// creates a new file
int CR(char *filepath, int size){
  // reads the state of the disk from myfs.
  read_state();
  // check to see is the file is under 8KB
  if(size > 8*BLOCK_SIZE){
    printf("No file greater than 8KB. \n");
    return -1;
  }
  char pathCopy[100];
  strcpy(pathCopy, filepath);
  char * child = strtok(pathCopy, "/");
  char *temp = child;
  int count = 0;
  //looping over the path to isolate the child directory needed to be created.
  while(temp != NULL){
      child = temp;
      count ++;
      temp = strtok(NULL, "/");
  }
  if(strlen(child) > 8){
    //check to make sure that the file name is under 8 char
    printf("Filename too large to handle. \n");
    return -1;
  }
  int parentBlk;
  if (count == 1){ //if parent is root
    parentBlk = 0;
  }
  else{
    strcpy(pathCopy, filepath);
    // get blk of parent
    parentBlk = getParentBlk(pathCopy);
    if(parentBlk == -1){ //if parent does not exits
      return -1;
    }
  }
  // check if file already exists
  for (int i = 0; i < 64; i++){
    if (strcmp(myDisk.myDatablk[parentBlk].directory[i].name, child)==0){
      printf("the file already exists.\n");
      return -1;
    }
  }
  // write to file
  int freeblock = 0;
  int block_list[8];
  int blockCount = 0;
  // if file is greater than 1024 bytes more than one free datablock is searched
  if (size > BLOCK_SIZE){
    for(int i =0 ; i < floor(size/BLOCK_SIZE)+1 ; i++){
      block_list[i] = getFreeBlock();
      if(block_list[i]< 0){
        printf("not enough space\n");
        return -1;
      }
    }
  }
  else{
    blockCount ++;
    freeblock = getFreeBlock();
    if(freeblock< 0){
      printf("not enough space\n");
      return -1;
    }
  }
  // searching for a unused inode in the superblock of the disk.
  int freeInode = getFreeInode();
  if (freeInode == -1){
    return -1;
  }
  int i = 0;
  while(strcmp(myDisk.myDatablk[parentBlk].directory[i].name, "") != 0){
    //check to see if the directory contains the new file beforehand
    if(strcmp(myDisk.myDatablk[parentBlk].directory[i].name, child) == 0){
      printf("the file already exists\n");
      return -1;
    }
      i++;
  }
  // when file content in one block
  if(blockCount > 0){
      for(int k =0 ; k< size; k++){
        // populating the file with the english alphabets
         myDisk.myDatablk[freeblock].file[k] = FILE_CONTENT[k%26];
    }
    int blk_ptr = 0;
    //marking the free inode index as  used
    while (myDisk.mySuperblk.inodeTable[freeblock].blockptrs[blk_ptr]!=-1) blk_ptr++;
    myDisk.mySuperblk.inodeTable[freeblock].blockptrs[blk_ptr] = freeblock;
    myDisk.mySuperblk.freeBlkList[freeblock] = 1;
  } 
  else{
    // when file content in more than one blk
      int currentSize = 0;
      for (int j = 0; j < floor(size/BLOCK_SIZE)+1; j++){
        for(int k =0 ; k< BLOCK_SIZE; k++){
          // populating the file with english alphabets.
          myDisk.myDatablk[block_list[j]].file[k] = FILE_CONTENT[k%26];
          currentSize++;
          if(currentSize == size){
            // breaks off the loop as soon as the size  of the file is met.
            break;
          }
      }
    }  
    int blk_ptr=0;
    for(int j = 0; j < floor(size/BLOCK_SIZE)+1; j++){
      // making all the pointers point towards the datablock containing the file.
    while (myDisk.mySuperblk.inodeTable[block_list[j]].blockptrs[blk_ptr]!=-1) {
      blk_ptr++;
    }
      myDisk.mySuperblk.inodeTable[block_list[j]].blockptrs[blk_ptr]=freeblock;
      myDisk.mySuperblk.freeBlkList[block_list[j]] = 1;
    }
  }
 
 // making a dirent entry in the datablock list
  strcpy(myDisk.myDatablk[parentBlk].directory[i].name, child);
  myDisk.myDatablk[parentBlk].directory[i].namelen = strlen(child);
  myDisk.myDatablk[parentBlk].directory[i].inode = freeInode;
  //marking the free inode index as  used
  myDisk.mySuperblk.inodeTable[freeInode].used = 1;
  myDisk.mySuperblk.inodeTable[freeInode].dir = 0;
  strcpy(myDisk.mySuperblk.inodeTable[freeInode].name , child);  
  myDisk.mySuperblk.inodeTable[freeInode].size= size;

  // update the parent directory size
  int parentInode;
  if (parentBlk == 0){
    parentInode = 0;
  }
  else parentInode = getParentInode(filepath);
  myDisk.mySuperblk.inodeTable[parentInode].size += sizeof(dirent);
  
  // updates the state of the disk in myfs
  update_state();
  return 0;  

}

// copies the file from src to dest
int CP(char *srcname, char *destname){
  // reads the state of the disk from myfs.
  read_state();
  char filecopy[100];
  strcpy(filecopy, srcname);
  char *child = strtok(filecopy, "/");
  char *temp = child;
  int check = 0;
  //find file to be copied
  while(temp != NULL){
      child = temp;
      check ++;
      temp = strtok(NULL, "/");
  }
  int parentBlk;
  if (check == 1){ //if root
    parentBlk = 0;
  }
  else{
    // find blk of the parent.
      parentBlk = getParentBlk(srcname);
      if (parentBlk < 0){
        printf("the directory in the given path doesnot exist \n");
        return -1;
      }
  }
  int* src_blks;
  int i;
  int size = -1;
  for (i =0; i < 64;i++){
    if (strcmp(myDisk.myDatablk[parentBlk].directory[i].name, child)==0){
      // if the given source is a directory copy return -1.
      if(myDisk.mySuperblk.inodeTable[myDisk.myDatablk[parentBlk].directory[i].inode].dir == 1){
        printf("can't handle directories\n");
        return -1;
      }
      // saving the size and the blockpointer of the source file
      src_blks = myDisk.mySuperblk.inodeTable[myDisk.myDatablk[parentBlk].directory[i].inode].blockptrs;
      size = myDisk.mySuperblk.inodeTable[myDisk.myDatablk[parentBlk].directory[i].inode].size;
      break;
    }
  }

  if(size == -1){
    printf("the directory in the given path doesnot exists\n");
    return -1;
  }
  // finding a free inode  in the super block.
  int freeInode = getFreeInode();
  if (freeInode==-1){
    return -1;
  }
  // creating the source file in the destination 
  int cr = CR(destname, size);
  if (cr == -1) return -1;
  int* dest_blks = myDisk.mySuperblk.inodeTable[freeInode].blockptrs;
  i = 0;
  while (src_blks[i]!=-1) {
    strcpy(myDisk.myDatablk[dest_blks[i]].file, myDisk.myDatablk[src_blks[i]].file);
    i++;
  }

// updates the state of the disk in myfs
  update_state();
  return 0;
}

// deletes a given file.
int DL(char* path) {
  // reads the state of the disk from myfs.
  read_state();
  char filecopy[100];
  strcpy(filecopy, path);
  char * child = strtok(filecopy, "/");
  char *temp = child;
  int check = 0;
  //find filename to be deleted
  while(temp != NULL){
      child = temp;
      check ++;
      temp = strtok(NULL, "/");
  }
  int dirIdx;
  if (check == 1){
    // checks if the  file is in root directory
    dirIdx = 0;
  }
  else{
    // gets the parent dirrectory's dirent index.
      dirIdx = getParentBlk(path);
      if(dirIdx < 0){
        printf("the directory %s in the given path does not exist \n", path);
        return -1;
      }
  }
  int* blks;
  int i;
  int file_inode;
  check = -1;
  for (i =0; i < 64;i++){
    // looping over the directory array in the parent directory to find the file.
    if (strcmp(myDisk.myDatablk[dirIdx].directory[i].name, child)==0){
      check ++;
    // saving the file's information.
      file_inode = myDisk.myDatablk[dirIdx].directory[i].inode;
      blks = myDisk.mySuperblk.inodeTable[file_inode].blockptrs;
      // updating the datablock in the  list.
      strcpy(myDisk.myDatablk[dirIdx].directory[i].name, "");
      myDisk.myDatablk[dirIdx].directory[i].inode=-1;
      myDisk.myDatablk[dirIdx].directory[i].namelen=0;
      break;
    }
  }
  if(check == -1){  
    // if the file  was not present in the given directory.
    printf("the file doesnot exist\n");
    return -1;
  }
  int j = 0;
  //freeing the datablk ptrs for the deleted file
  while (blks[j]!=-1) {
    strcpy(myDisk.myDatablk[blks[j]].file, "");
    myDisk.mySuperblk.freeBlkList[j]=0;
    myDisk.mySuperblk.inodeTable[file_inode].blockptrs[j]=-1;
    j++;
  }
  // updating the superblock
  strcpy(myDisk.mySuperblk.inodeTable[file_inode].name,"");
  myDisk.mySuperblk.inodeTable[file_inode].used = 0;
  myDisk.mySuperblk.inodeTable[file_inode].dir = -1;
  myDisk.mySuperblk.inodeTable[file_inode].rsvd = 0;
  myDisk.mySuperblk.inodeTable[file_inode].size = 0;
  // update the parent directory size
  int parentInode;
  if (dirIdx == 0){
    parentInode=0;
  }
  else parentInode=getParentInode(path);
  myDisk.mySuperblk.inodeTable[parentInode].size -= sizeof(dirent);

  //updates the state of the disk in myfs
  update_state();
  return 0;
}

// moves file from source to destination
int MV(char *srcname, char *destname){
  //reads the state of the disk from myfs
  read_state();
	char srcCopy[100];
  char srcCopy2[100];
  strcpy(srcCopy, srcname);
  strcpy(srcCopy2, srcname);
  char *child = strtok(srcCopy, "/");
  char *temp = child;
  int check = 0;
  //looping over the src to isolate the child
  while(temp != NULL){
      child = temp;
      check ++;
      temp = strtok(NULL, "/");
  }
  int parentBlk;
  if (check == 1){
    parentBlk = 0;
  }
  else{
      parentBlk = getParentBlk(srcname);
      if(parentBlk < 0){
        printf("The directory in te given path does not exist \n");
        return -1;
      }
  }
  int i;
  int size = -1;
  for (i =0; i < 64;i++){
    //finding the file in the parent directory's directory list to save the size.
    if (strcmp(myDisk.myDatablk[parentBlk].directory[i].name, child)==0){
      if(myDisk.mySuperblk.inodeTable[myDisk.myDatablk[parentBlk].directory[i].inode].dir == 1){
        printf("can't handle directories\n");
        return -1;
      }
      size = myDisk.mySuperblk.inodeTable[myDisk.myDatablk[parentBlk].directory[i].inode].size;
      break;
    }
  }
  if(size == -1){
    printf("The directory in te given path does not exist \n");
    return -1;
  }
  else{
    //create a new file  in the destination
    CR(destname, size);
    // delete the file from the source
    DL(srcCopy2);
  }
  // update the state of myfs.
  update_state();
  return 0;
}

//delete directory
int DD(char* path) {
  //reads the state of the disk from myfs
  read_state();
  char filecopy[100];
  strcpy(filecopy, path);
  char * child = strtok(filecopy, "/");
  char *temp = child;
  int check = 0;
  //find child
  while(temp != NULL){
      child = temp;
      check ++;
      temp = strtok(NULL, "/");
  }
  char dirname[8];
  strcpy(dirname, child);
  int dirIdx;
  if (check == 1){
    dirIdx = 0;
  }
  else{
      dirIdx = getParentBlk(filecopy);
  }
  if (dirIdx==-1) return 1;
  int dir_blk, dir_inode;
  char childpath[100];
  strcpy(childpath, path);
  
  int dirindex = 0; // dirent of child in parent
  while(strcmp(myDisk.myDatablk[dirIdx].directory[dirindex].name, child)!=0 && dirindex<BLOCK_SIZE){
    dirindex ++;
  }
  if (dirindex > -1){
    dir_inode = myDisk.myDatablk[dirIdx].directory[dirindex].inode; // child directory inode
    dir_blk = myDisk.mySuperblk.inodeTable[dir_inode].blockptrs[0]; // child directory datablock

    for (int k = 0; k < 10; k++){
      // looking for all the subdirectory and files of the child directory
      if (myDisk.mySuperblk.inodeTable[myDisk.myDatablk[dir_blk].directory[k].inode].dir == 1 && myDisk.myDatablk[dir_blk].directory[k].inode!=0 && myDisk.myDatablk[dir_blk].directory[k].inode!=-1){
        strcpy(childpath, path);
        strcat(childpath,"/");
        strcat(childpath,myDisk.mySuperblk.inodeTable[myDisk.myDatablk[dir_blk].directory[k].inode].name);
        // calling delete dir on the subdirectory.
        DD(childpath);
      }
      else if (myDisk.mySuperblk.inodeTable[myDisk.myDatablk[dir_blk].directory[k].inode].dir == 0 && myDisk.myDatablk[dir_blk].directory[k].inode != -1){
        strcpy(childpath,path);
        strcat(childpath,"/");
        strcat(childpath,myDisk.mySuperblk.inodeTable[myDisk.myDatablk[dir_blk].directory[k].inode].name);
        // calling delete file on the files inside the child directory
        DL(childpath);
      }
    }
    int ch;
    for (ch=0; ch< 64; ch++){
      if (myDisk.myDatablk[dir_blk].directory[ch].namelen != 0){
        // delete dirent of child from the parent directory
        myDisk.myDatablk[dir_blk].directory[ch].inode = -1;
        strcpy(myDisk.myDatablk[dirIdx].directory[ch].name,"");
        myDisk.myDatablk[dir_blk].directory[ch].namelen = 0;
      }
    }
    myDisk.mySuperblk.freeBlkList[dir_blk] = 0;
    // delete inode
    myDisk.mySuperblk.inodeTable[dir_inode].dir=-1;
    myDisk.mySuperblk.inodeTable[dir_inode].used=0;
    myDisk.mySuperblk.inodeTable[dir_inode].size=0;
    myDisk.mySuperblk.inodeTable[dir_inode].rsvd=0;
    strcpy(myDisk.mySuperblk.inodeTable[dir_inode].name,"");
    for (int k =0; k<8; k++){
      if (myDisk.mySuperblk.inodeTable[dir_inode].blockptrs[k] !=-1){
        myDisk.mySuperblk.inodeTable[dir_inode].blockptrs[k]=-1;
      }
    }
    // delete dirent from parent
    myDisk.myDatablk[dirIdx].directory[dirindex].inode = -1;
    strcpy(myDisk.myDatablk[dirIdx].directory[dirindex].name,"");
    myDisk.myDatablk[dirIdx].directory[dirindex].namelen=0;
    // update the parent directory size
    int parentInode;
    if (dirIdx == 0){
      parentInode=0;
    }
    else parentInode = getParentInode(path);
    myDisk.mySuperblk.inodeTable[parentInode].size -= sizeof(dirent);
  }
  else {
    return -1;
  }
  //update the state in myfs
  update_state();
  return 0;
  
}

void LL(){
  // read the state of the disk
    read_state();
    // prints the inode list from the superblock
    for(int i=0; i<16; i++){
	  if(myDisk.mySuperblk.inodeTable[i].used == 1){
		  printf("%s %d\n", myDisk.mySuperblk.inodeTable[i].name, myDisk.mySuperblk.inodeTable[i].size);
	  }
  }
}


/*
 * main
 */
int main (int argc, char* argv[]) {

  int check = initialize();
  if(check == 0){
      myfs = fopen("./myfs", "w");
  fclose(myfs);
  }

  FILE *stream;
  char line[200];
  char *command, *path1, *path2, *test;
  int size;

    if((stream=fopen(argv[1], "r")) == NULL) {
      printf("Cannot open input file %s for reading.\n", argv[1]);
      exit(1);
    }

    while (fgets(line, 200 ,stream)){
      // checking for LL command which has no parameters 
      if (strlen(line) == 2||strlen(line) == 3) { 
        command = strtok(line, "\n");
        if (strcmp(command,"LL") == 0)
          LL();
      }
      else {
        // saving command name in command
        command = strtok(line, " ");
        // if command is for create file
        if (strcmp(command,"CR") == 0){
          path1 = strtok(NULL, " ");
          test = strtok(NULL, "\n");
          if(test == NULL){
            printf("File size not specified \n");
          }
          else{
            size = atoi(test);	//convert size into int
            CR(path1, size);
          }
        }
        // if command is for move file
        else if (strcmp(command, "MV")==0){
          path1 = strtok(NULL, " ");
          path2 = strtok(NULL, "\n");
          if(path2 == NULL){
            printf("Destination not specified \n");
          }
          MV(path1, path2);
        }
        // if command is for copy file
        else if (strcmp(command, "CP")==0){
          path1 = strtok(NULL, " ");
          path2 = strtok(NULL, "\n");
          if(path2 == NULL){
            printf("Destination not specified \n");
          }
          CP(path1, path2); 
        }
        // if command is for delete file
        else if (strcmp(command,"DL")==0){
            path1 = strtok(NULL, "\n");
            DL(path1);
        }
        // if command is for create dir
        else if (strcmp(command,"CD")==0){
            path1 = strtok(NULL, "\n");
            CD(path1);
        }
        // if command is for delete dir
        else if (strcmp(command,"DD")==0){
            path1 = strtok(NULL, "\n");
            DD(path1);
          }
        }
    }
  fclose(stream);
	return 0;
}

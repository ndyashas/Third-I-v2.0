#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#define BLOCKSZ 4096
#define MAXIN 30
#define MAXDN 300
#define INDISKSZ 512
#define DNDISKSZ 1024
#define DPERN 25


// Currently total disk size is 1.2 MB
const int DISKSIZE = (2 * BLOCKSZ) + (MAXIN * INDISKSZ) + (MAXDN * DNDISKSZ);

typedef struct INODE{
    char path[50];                    // Path upto node
    char name[20];                    // Name of the file / directory
    char type;                      // Type : "directory" or "file"
    mode_t permissions;		        // Permissions 
    uid_t user_id;		            // userid
    gid_t group_id;		            // groupid
    int num_children;               // Number of children nodes
    int num_files;		            // Number of files
    time_t a_time;                  // Access time
    time_t m_time;                  // Modified time
    time_t b_time;                  // Creation time
    off_t size;                     // Size of the node
    unsigned long int i_number;     // Inode number of the node in disk    
    struct INODE * parent;          // Pointer to parent node
    long int children[30];       // Pointers to children nodes
    char * data;                    // data stored in the directory
	long int datab[DPERN];                     // Track of data blocks
}INODE;

INODE *ROOT;
unsigned long int IN = 0;
int HDISK;
char *IBMAP;
char *DBMAP;

const mode_t USR_NPF = S_IRUSR | S_IWUSR; 
const mode_t USR_NPD = S_IRUSR | S_IWUSR | S_IFDIR;
const mode_t GRP_NPF = S_IRGRP | S_IWGRP;
const mode_t GRP_NPD = S_IRGRP | S_IWGRP | S_IFDIR;

char * reverse(char *, int);
char * getName(char **);
char * getDir(char *);
INODE * getNodeFromPath(char *, INODE *);
int delNode(char *);
int addNode(char *, char);


INODE * initializeNode( char *, char *, char,  INODE *);
void initializeTIFS(INODE **);


int ti_getattr(const char *, struct stat *);
int ti_readdir(const char *, void *, fuse_fill_dir_t , off_t , struct fuse_file_info *);
int ti_mkdir(const char *, mode_t );
int ti_rmdir(const char *);
int ti_open(const char *, struct fuse_file_info *);
int ti_mknod(const char *, mode_t, dev_t);
int ti_write(const char *, const char *, size_t , off_t , struct fuse_file_info *);
int ti_read(const char *, char *, size_t , off_t ,struct fuse_file_info *);
int ti_unlink(const char *);
int ti_chmod(const char *, mode_t );
int ti_truncate(const char *, off_t );
int ti_access(const char *, int);
int ti_utime(const char *, struct utimbuf *);

static struct fuse_operations operations = {
	.getattr	= ti_getattr,
	.readdir	= ti_readdir,
	.mkdir      = ti_mkdir,
	.rmdir      = ti_rmdir,
	.open       = ti_open,
	.mknod      = ti_mknod,
	.write		= ti_write,
	.read		= ti_read,
    .unlink     = ti_unlink,
	.chmod		= ti_chmod,
	.truncate   = ti_truncate,
	.access	    = ti_access,
	.utime      = ti_utime,
};

int main( int argc, char *argv[] ){
	initializeTIFS(&ROOT);
    return fuse_main(argc, argv, &operations);
}


void openDisk(){
	HDISK = open("metaFiles/HDISK.meta", O_RDWR | O_CREAT , S_IRUSR | S_IWUSR);
}

void closeDisk(){
	close(HDISK);
}


int getInodeNumber(){
	// printf("getInodeNumber called\n");
	int ino;
	openDisk();
	lseek(HDISK, 0, SEEK_SET);
	memset(IBMAP, '0', MAXIN);
	read(HDISK, IBMAP, MAXIN);
	for(ino=0; ino<MAXIN; ino++) if(IBMAP[ino] == '0') break;
	IBMAP[ino] = '1';
	lseek(HDISK, 0, SEEK_SET);
	write(HDISK, IBMAP, MAXIN);
	//	closeDisk();
	return(ino);
}

void returnInodeNumber(int ino){
	// printf("returnInodeNumber called\n");
	openDisk();
	lseek(HDISK, 0, SEEK_SET);
	memset(IBMAP, '0', MAXIN);
	read(HDISK, IBMAP, MAXIN);
	IBMAP[ino] = '0';
	lseek(HDISK, 0, SEEK_SET);
	write(HDISK, IBMAP, MAXIN);
	//	closeDisk();
}

int getDnodeNumber(){
	int dno;
	//memset(DBMAP, '0', MAXDN);
	openDisk();
	lseek(HDISK, MAXIN, SEEK_SET);
	read(HDISK, DBMAP, MAXDN);
	for(dno=1; dno<MAXDN; dno++) if(DBMAP[dno] == '0') break;
	DBMAP[dno] = '1';
	lseek(HDISK, MAXIN, SEEK_SET);
	write(HDISK, DBMAP, MAXDN);
	return(dno);
}

void returnDnodeNumber(int dno){
	// printf("returnDnodeNumber called\n");
	openDisk();
	lseek(HDISK, MAXIN, SEEK_SET);
	//memset(DBMAP, '0', DNDISKSZ);
	read(HDISK, DBMAP, MAXDN);
	DBMAP[dno] = '0';
	lseek(HDISK, MAXIN, SEEK_SET);
	write(HDISK, DBMAP, MAXDN);
	//	closeDisk();
}


void clearInode(int ino){
	int offset = (MAXIN + MAXDN) + (ino * INDISKSZ);
	char *buff = (char*)malloc(sizeof(char)*INDISKSZ);
	memset(buff, 0, INDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	write(HDISK, buff, INDISKSZ);
	//	closeDisk();
}

void clearDnode(int dno){
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) +(dno * DNDISKSZ);
	char *buff = (char*)malloc(sizeof(char)*DNDISKSZ);
	memset(buff, 0, DNDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	write(HDISK, buff, DNDISKSZ);
	//	closeDisk();
}

void storeDnode(char *data, int dno){
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) + (dno * DNDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	int w = write(HDISK, data, DNDISKSZ);
	// printf("%d bytes written from %s\n", w, data);
	// closeDisk();
}

char * getDnode(int dno){
	char *toret = (char*)malloc(sizeof(char)*DNDISKSZ);
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) + (dno * DNDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	read(HDISK, toret, DNDISKSZ);
	return(toret);
}

void storeInode(INODE *nd){
	// printf("storeInode() called\n");
	if(nd == NULL) return;
	int offset = (MAXIN + MAXDN) + (nd->i_number * INDISKSZ);
	off_t bw;
	// printf("TILL HERE\n");
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	write(HDISK, nd->path, sizeof(nd->path));
	write(HDISK, nd->name, sizeof(nd->name));
	write(HDISK, &(nd->type), sizeof(nd->type));
	write(HDISK, &(nd->permissions), sizeof(nd->permissions));
	write(HDISK, &(nd->user_id), sizeof(nd->user_id));
	write(HDISK, &(nd->group_id), sizeof(nd->group_id));
	write(HDISK, &(nd->num_children), sizeof(nd->num_children));
	write(HDISK, &(nd->num_files), sizeof(nd->num_files));
	write(HDISK, &(nd->a_time), sizeof(nd->a_time));
	write(HDISK, &(nd->m_time), sizeof(nd->m_time));
	write(HDISK, &(nd->b_time), sizeof(nd->b_time));
	bw = lseek(HDISK, 0, SEEK_CUR);
	if(nd->type == 'f'){
		if(nd->data == NULL) {nd->data = (char*)malloc(sizeof(char)); nd->data[0] = 0;}
		int dno = 0, dsize = strlen(nd->data), i = 0, doff = 0;
		for(i=0; i<DPERN; i++){
			if(dsize > 0){
				returnDnodeNumber(nd->datab[i]);
				clearDnode(nd->datab[i]);
				dsize -= DNDISKSZ;
			}
		}
		nd->size = 0;
		printf("Data size of %s is %ld\n", nd->name, strlen(nd->data));
		for(dsize = strlen(nd->data), i=0, doff = 0; dsize > 0; dsize -= DNDISKSZ, doff += DNDISKSZ, i++){
			dno = getDnodeNumber();
			storeDnode(nd->data + doff, dno);
			nd->datab[i++] = dno;
			nd->size += DNDISKSZ;
		}
		lseek(HDISK, bw, SEEK_SET);
		write(HDISK, &(nd->datab), sizeof(nd->datab));
	}
	else
		write(HDISK, nd->children, sizeof(nd->children));
	write(HDISK, &(nd->size), sizeof(nd->size));
	closeDisk();
}


INODE * getInode(int ino){
	INODE *toret = (INODE *)malloc(sizeof(INODE));
	char *buff = (char*)malloc(sizeof(char)*INDISKSZ);
	openDisk();

	toret->i_number = ino;
	// printf("Getting data for %d\n", ino);
	int offset = (MAXIN + MAXDN) + (ino * INDISKSZ);
	lseek(HDISK, offset, SEEK_SET);
	read(HDISK, buff, sizeof(toret->path));
	memcpy(toret->path, buff, sizeof(toret->path));
	read(HDISK, buff, sizeof(toret->name));
	memcpy(toret->name, buff, sizeof(toret->name));
	read(HDISK, buff, sizeof(toret->type));
	toret->type = *((char *)buff);
	read(HDISK, buff, sizeof(toret->permissions));
	toret->permissions = *((mode_t *)buff);
	read(HDISK, buff, sizeof(toret->user_id));
	toret->user_id = *((uid_t *)buff);
	read(HDISK, buff, sizeof(toret->group_id));
	toret->group_id = *((uid_t *)buff);
	read(HDISK, buff, sizeof(toret->num_children));
	toret->num_children = *((int *)buff);
	read(HDISK, buff, sizeof(toret->num_files));
	toret->num_files = *((int *)buff);
	read(HDISK, buff, sizeof(toret->a_time));
	toret->a_time = *((time_t *)buff);
	read(HDISK, buff, sizeof(toret->m_time));
	toret->m_time = *((time_t *)buff);
	read(HDISK, buff, sizeof(toret->b_time));
	toret->b_time = *((time_t *)buff);
	if(toret->type == 'f'){
		read(HDISK, buff, sizeof(toret->datab));
		memcpy(toret->datab, buff, sizeof(toret->datab));
	}
	else{
		read(HDISK, buff, sizeof(toret->children));
		memcpy(toret->children, buff, sizeof(toret->children));
	}
	read(HDISK, buff, sizeof(toret->size));
	toret->size = *((off_t *)buff);
	free(buff);
	if(toret->type == 'f'){
		int i;
		toret->data = (char*)malloc(sizeof(char) * (toret->size + 1));
		for(i=0; i<toret->size; i += DNDISKSZ){
			strcat(toret->data, getDnode(toret->datab[i]));
		}
	}
	// printf("Done getting data for %d\n", ino);
	closeDisk();
	return(toret);
}


char * reverse(char * str, int mode){
    int i;
    int len = strlen(str);
    char * retval = (char *)calloc(sizeof(char), (len + 1));
    for(i = 0; i <= len/2; i++){
        retval[i] = str[len - 1 -i];
        retval[len - i - 1] = str[i];
    }
    if(retval[0] == '/' && mode == 1) {   // if mode is set to 1, then drop the leading '/'. Set to 0 for normal reversing.
        retval++;
    }
    return retval;
}

char * getName(char ** copy_path){
    char * retval = (char *)calloc(sizeof(char), 1);
    int retlen = 0;
    char temp;
    char * tempstr;
    *copy_path = reverse(*copy_path, 1);    // change "a/b/c" to "c/b/a" and extract content upto the first '/'
    temp = **(copy_path);
    while(temp != '/'){    
        tempstr = (char *)calloc(sizeof(char), (retlen + 2));
        strcpy(tempstr, retval);
        retlen += 1;
        tempstr[retlen - 1] = temp;
        retval = (char *)realloc(retval, sizeof(char) * (retlen + 1));
        strcpy(retval, tempstr);
        (*copy_path)++;
        temp = **(copy_path);
        free(tempstr);
    }
    if(strlen(*copy_path) > 1){
        (*copy_path)++;                     // remove the leading '/' from "/b/a" after extracting 'c'
    }
    retval = (char *)realloc(retval, sizeof(char) * (retlen + 1));
    retval[retlen] = '\0';
    retval = reverse(retval, 0);        
    *(copy_path) = reverse(*(copy_path), 0);
    return retval;
}


char * getDir(char * apath){
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	// printf("getDir() called with path %s\n", path);
	if(path == NULL) return(NULL);
	char *dirp = reverse((char*)path, 1);
	while(dirp[0] != '/') dirp++;
	return(reverse(dirp, 0));
}


INODE * getNodeFromPath(char * apath, INODE *parent){
	// printf("getNodeFromPath called with path %s\n", apath);
	int i;
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	if((path[strlen(path)-1] == '/') && (strcmp("/", path) != 0)) path[strlen(path) - 1] = '\0';
	INODE *retn;
	if((path == NULL)||(parent == NULL)||(strcmp(path, "") == 0)) return(NULL);
	// printf("comparing %s with %s\n", parent->path, path);
	if(strcmp(parent->path, path) == 0) return(parent);
	// printf("Comparision failed\n");
	
	for(i=0; i<parent->num_children; i++){
		// printf("Searching with child %d\n", i);
		retn = getNodeFromPath(path, getInode((parent->children)[i]));
		if(retn != NULL){/*printf("GOT NODE\n");*/return(retn);}
	}
	// printf("NODE NOT FOUND\n");
	return(NULL);
}


int delNode(char *apath){
	// printf("delNode() called with path %s\n", apath);
	int i, flag = 0;
	if(apath == NULL) return(0);
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	INODE *parent = getNodeFromPath(getDir(path), ROOT);
	if(parent == NULL) return(0);
	for(i=0; i<parent->num_children; i++){
		INODE *child = getInode(parent->children[i]);
		if(strcmp(child->path, path) == 0){
			if((child->type == 'd') && (child->num_children != 0)) return(-ENOTEMPTY);
			returnInodeNumber(child->i_number);
			clearInode(child->i_number);
			// free(child);
			// parent->children[i] = (INODE *)malloc(sizeof(INODE));
			flag = 1;
		}
		if(flag){
			if(i != parent->num_children-1)
				parent->children[i] = parent->children[i+1];
		}
	}
	parent->num_children -= 1;
	storeInode(parent);
	storeInode(ROOT);
	/* printf("INode number of parent %ld\n", parent->i_number); */
	/* printf("Path of parent %s\n", parent->path); */
	/* printf("Name of parent %s\n", parent->name); */
	/* printf("Number of children to parent %d\n", parent->num_children); */
	/* for(i=0; i<parent->num_children;i++) printf(" %d --", parent->children[i]); */
	/* printf("\n"); */
	free(path);
	return(0);
}

int addNode(char * apath, char type){
	// printf("addNode() called with path %s\n", apath);
	if(apath == NULL) return(0);
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	INODE *parent = getNodeFromPath(getDir(path), ROOT);
	if(parent == NULL) return(0);
	/*if(parent->children == NULL){
		parent->num_children = 0;
		parent->children = (INODE **)malloc(sizeof(INODE*));
		}*/
	parent->num_children += 1;
	// parent->children = (INODE **)realloc(parent->children, sizeof(INODE *) * parent->num_children);
	// parent->children[parent->num_children - 1] = initializeNode(apath, getName(&path), type, parent);
	INODE *child = initializeNode(apath, getName(&path), type, parent);
	parent->children[parent->num_children - 1] = child->i_number;
	storeInode(parent);
	storeInode(child);
	storeInode(ROOT);
	/* printf("Name of the node is %s\n", getInode(parent->children[parent->num_children - 1])->name); */
	/* printf("Path of the node is %s\n", getInode(parent->children[parent->num_children - 1])->path); */
	/* printf("Inode of the node is %ld\n", getInode(parent->children[parent->num_children - 1])->i_number); */
	/* printf("Name of parent is %s\n", parent->name); */
	/* printf("Path of parent is %s\n", parent->path); */
	/* printf("Inode of parent is %ld\n", parent->i_number); */
	/* printf("Number of children to parent %d\n", parent->num_children);	 */
	return(0);
}


INODE * initializeNode( char *path, char *name, char type, INODE *parent){
	INODE *ret = (INODE*)malloc(sizeof(INODE));
	//ret->path = (char*)malloc(sizeof(char)*strlen(path));
	strcpy(ret->path, path);
	//ret->name = (char*)malloc(sizeof(char)*strlen(name));
	strcpy(ret->name, name);
	ret->type = type;
	if(type == 'd'){  
        ret->permissions = S_IFDIR | 0755;
		ret->size = INDISKSZ;
    }     
    else{
    	ret->permissions = S_IFREG | 0644;
		ret->size = 0;
    } 
	ret->user_id = getuid();
	ret->group_id = getgid();
	ret->num_children = 0;
	ret->num_files = 0;
	time(&(ret->a_time));
	time(&(ret->m_time));
	time(&(ret->b_time));
	ret->i_number = getInodeNumber();
	// printf("Inode number is %ld\n", ret->i_number);
	ret->parent = parent;
	// ret->children = NULL;
	ret->data = NULL;
	//ret->datab = NULL;
	return(ret);
}


void initializeTIFS(INODE **rt){
	// printf("Initializing TIFS\n");
	IBMAP = (char*)malloc(sizeof(char)*(MAXIN));
	DBMAP = (char*)malloc(sizeof(char)*(MAXDN));
	if(access("metaFiles/HDISK.meta", F_OK ) != -1){
		// printf("Loading data from disk ...\n");
	}
	
	else{
		// printf("No old meta files found initializing new TIFS\n");
		char *buf = (char*)malloc(sizeof(char)*DISKSIZE);
		openDisk();
		memset(buf, 0, DISKSIZE);
		write(HDISK, buf, DISKSIZE);

		memset(buf, '0', DISKSIZE);
		lseek(HDISK, 0, SEEK_SET);
		write(HDISK, buf, MAXIN);
		write(HDISK, buf, MAXDN);
		closeDisk();
		
		(*rt) = initializeNode( "/", "TIROOT", 'd',  (*rt));
		storeInode((*rt));
	}
	(*rt) = getInode(0);
}


int ti_getattr(const char *apath, struct stat *st){
	// printf("ti_getattr() called with path %s\n", apath);
	INODE *nd = getNodeFromPath((char *)apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->type == 'd') st->st_nlink = 2;
	else st->st_nlink = 1;
	
	/* printf("**********************\n"); */
	/* printf("Inode number %ld\n", nd->i_number); */
	/* printf("Name %s\n", nd->name); */
	/* printf("Path %s\n", nd->path); */
	/* printf("Number of children %d\n", nd->num_children); */
	/* if(nd->type == 'f') */
	/* 	printf("Data of file : %s", nd->data); */
	/* else */
	/* 	for(int i=0; i<nd->num_children; i++) printf(" %ld --", nd->children[i]); */
	/* printf("\n**********************\n"); */
	
	
	st->st_ino = nd->i_number;
	st->st_nlink += nd->num_children;
	st->st_mode = nd->permissions;
	st->st_uid = nd->user_id; 
	st->st_gid = nd->group_id;
	st->st_atime = nd->a_time;
	st->st_mtime = nd->m_time;
	st->st_size = nd->size;
	st->st_blocks = (((nd->size) / 512) + 1);
	return(0);
}

int ti_readdir(const char *apath, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	// printf("ti_readdir() called with path %s\n", apath);
	filler(buffer, ".", NULL, 0 ); 
	filler(buffer, "..", NULL, 0 );
	
	INODE *nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	
	nd->a_time=time(NULL);
	for(int i = 0; i < nd->num_children; i++){
		filler( buffer, getInode(nd->children[i])->name, NULL, 0 );
	}
	return(0);
}


int ti_mkdir(const char * apath, mode_t x){
	// printf("ti_mkdir() called with path %s\n", apath);
	int retVal = addNode((char*) apath, 'd');
	return(retVal);
}


int ti_rmdir(const char * apath){
	// printf("ti_mkdir() called with path %s\n", apath);
	int retVal = delNode((char *) apath);
	return(retVal);
}


int ti_open(const char *apath, struct fuse_file_info *fi){
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	return(0);
}


int ti_mknod(const char * apath, mode_t x, dev_t y){
	// printf("ti_mknod() called with path %s\n", apath);
	int retVal = addNode((char*) apath, 'f');
	return(retVal);
}

int ti_write(const char *apath, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	nd->m_time = time(NULL);
	nd->a_time = time(NULL);
	nd->size = size + offset;
	if(nd->data == NULL)
		nd->data = (char *)malloc(sizeof(char)*(nd->size));
	else
		nd->data = (char *)realloc(nd->data, sizeof(char)*(nd->size));
	memcpy(nd->data + offset, buf, size);
	//nd->data[((nd->size)--) - 1] = '\0';
	storeInode(nd);
	return(size);
}


int ti_read(const char *apath, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->size == 0) return(0);
	if(nd->data == NULL) return(0);
	nd->a_time = time(NULL);
	memcpy(buf, nd->data + offset, nd->size);
	storeInode(nd);
	return(size);
}


int ti_unlink(const char *apath){
	// printf("ti_unlink() called with path %s\n", apath);
	int retVal = delNode((char *) apath);
	return(retVal);
}


int ti_chmod(const char *apath, mode_t new){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	nd->m_time = time(NULL);
	nd->permissions = new;
	storeInode(nd);
	return(0);
}


int ti_truncate(const char *apath, off_t size){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->size == 0) return(0);
	if(nd->data == NULL) return(0);
	nd->m_time = time(NULL);
	nd->a_time = time(NULL);
	free(nd->data);
	nd->data = NULL;
	storeInode(nd);
	return(0);
}


int ti_access(const char * apath, int mask){
	int grant = 1;		
	if(grant)
		return(0);
	return(-EACCES);
}

int ti_utime(const char *apath, struct utimbuf *tv){
	return 0;
}

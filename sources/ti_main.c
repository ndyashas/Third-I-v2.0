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


typedef struct INODE{
    char * path;                    // Path upto node
    char * name;                    // Name of the file / directory
    char type;                    // Type : "directory" or "file"
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
    struct INODE ** children;       // Pointers to children nodes
    char * data;          // data stored in the directory
}INODE;


INODE *ROOT;
unsigned long int IN = 1;
int BLOCKSZ = 4096;

mode_t USR_NPF = S_IRUSR | S_IWUSR; 
mode_t USR_NPD = S_IRUSR | S_IWUSR | S_IFDIR;
mode_t GRP_NPF = S_IRGRP | S_IWGRP;
mode_t GRP_NPD = S_IRGRP | S_IWGRP | S_IFDIR;

INODE * getNodeFromPath(char *, INODE *);

INODE * initializeNode( char *, char *, char,  INODE *);
void initializeTIFS(INODE **);


int ti_getattr(const char *, struct stat *);
int ti_readdir(const char *, void *, fuse_fill_dir_t , off_t , struct fuse_file_info *);
int ti_mkdir(const char *, mode_t );
int ti_rmdir(const char *);
int ti_open(const char *, struct fuse_file_info *);
int ti_unlink(const char *);
int ti_read(const char *, char *, size_t , off_t ,struct fuse_file_info *);
int ti_chmod(const char *, mode_t );
int ti_truncate(const char *, off_t );
int ti_write(const char *, const char *, size_t , off_t , struct fuse_file_info *);
int ti_access(const char *, int);
int ti_mknod(const char *, mode_t, dev_t);
int ti_utime(const char *, struct utimbuf *);

static struct fuse_operations operations = {
	.getattr	= ti_getattr,
	.readdir	= ti_readdir,
	.mkdir      = ti_mkdir,
	.rmdir      = ti_rmdir,
	.open       = ti_open,
	.read		= ti_read,
	.write		= ti_write,
	.mknod      = ti_mknod,
    .unlink     = ti_unlink,
	/*  .chmod		= ti_chmod,*/
	.truncate   = ti_truncate,
	.utime      = ti_utime,
	.access	    = ti_access
};

int main( int argc, char *argv[] ){
	initializeTIFS(&ROOT);
    return fuse_main(argc, argv, &operations);
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


INODE * getNodeFromPath(char * apath, INODE *root){
	// printf("getNodeFromPath called with path %s\n", apath);
	int i;
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	if((path[strlen(path)-1] == '/') && (strcmp("/", path) != 0)) path[strlen(path) - 1] = '\0';
	INODE *retn;
	if((path == NULL)||(root == NULL)||(strcmp(path, "") == 0)) return(NULL);	
	if(strcmp(root->path, path) == 0) return(root);

	for(i=0; i<root->num_children; i++){
		retn = getNodeFromPath(path, (root->children)[i]);
		if(retn != NULL)return(retn);
	}
	return(NULL);
}


int delNode(char *apath){
	int i, flag = 0;
	if(apath == NULL) return(0);
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	INODE *parent = getNodeFromPath(getDir(path), ROOT);
	if(parent == NULL) return(0);
	for(i=0; i<parent->num_children; i++){
		if(strcmp((parent->children[i])->path, path) == 0){
			if((parent->children[i]->type == 'd') && ((parent->children[i])->num_children != 0)) return(-ENOTEMPTY);
			free(parent->children[i]);
			parent->children[i] = (INODE *)malloc(sizeof(INODE));
			flag = 1;
		}
		if(flag){
			if(i != parent->num_children-1)
				parent->children[i] = parent->children[i+1];
		}
	}
	parent->num_children -= 1;
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
	if(parent->children == NULL){
		parent->num_children = 0;
		parent->children = (INODE **)malloc(sizeof(INODE*));
	}
	parent->num_children += 1;
	parent->children = (INODE **)realloc(parent->children, sizeof(INODE *) * parent->num_children);
	parent->children[parent->num_children - 1] = initializeNode(apath, getName(&path), type, parent);

	
	/* printf("Name of the node is %s\n", parent->children[parent->num_children - 1]->name); */
	/* printf("Path of the node is %s\n", parent->children[parent->num_children - 1]->path); */
	/* printf("Inode of the node is %d\n", parent->children[parent->num_children - 1]->i_number); */
	return(0);
}

/*
char * getData(INODE * root){
	if((root == NULL) || (root->datac == NULL) || (root->type == 'd')) return(NULL);
	DATAN *iter = NULL;
	int size = root->size;
	char *toret = (char *)malloc(sizeof(char)*(size+1));
	iter = root->datac;
	while(iter != NULL){
		strcat(toret, iter->data);
		iter = iter->next;
	}
	return(toret);
}
*/

INODE * initializeNode( char *path, char *name, char type, INODE *parent){
	INODE *ret = (INODE*)malloc(sizeof(INODE));
	ret->path = (char*)malloc(sizeof(char)*strlen(path));
	strcpy(ret->path, path);
	ret->name = (char*)malloc(sizeof(char)*strlen(name));
	strcpy(ret->name, name);
	ret->type = type;
	if(type == 'd'){  
        ret->permissions = S_IFDIR | 0755;
    }     
    else{
    	ret->permissions = S_IFREG | 0644; 
    } 
	ret->user_id = getuid();
	ret->group_id = getgid();
	ret->num_children = 0;
	ret->num_files = 0;
	time(&(ret->a_time));
	time(&(ret->m_time));
	time(&(ret->b_time));
	ret->size = 0;
	ret->i_number = IN++;
	ret->parent = parent;
	ret->children = NULL;
	ret->data = NULL;
	return(ret);
}

void initializeTIFS(INODE **rt){
	// printf("Initializing TIFS\n");
	if(access("metaFiles/hdisk", F_OK ) != -1){
	    // printf("Loading data from disk ...\n");
	}

	else{
		// printf("No old meta files found initializing new TIFS\n");
		(*rt) = initializeNode( "/", "TIROOT", 'd',  (*rt));
	}
}


int ti_getattr(const char *apath, struct stat *st){
	// printf("ti_getattr() called with path %s\n", apath);
	INODE *nd = getNodeFromPath((char *)apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->type == 'd') st->st_nlink = 2;
	else st->st_nlink = 1;

	printf("**********************\n");
	printf("Inode number %ld\n", nd->i_number);
	printf("Name %s\n", nd->name);
	printf("Path %s\n", nd->path);
	printf("Number of children %d\n", nd->num_children);
	printf("**********************\n");
	
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
		filler( buffer, nd->children[i]->name, NULL, 0 );
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
	nd->size = size + offset;
    memcpy(nd->data + offset, buf, size);
	return(size);
}


int ti_read(const char *apath, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->size == 0) return(0);
	memcpy(buf, nd->data + offset, nd->size);
	return(size);
}


int ti_unlink(const char *apath){
	// printf("ti_unlink() called with path %s\n", apath);
	int retVal = delNode((char *) apath);
	return(retVal);
}


int ti_chmod(const char *apath, mode_t new){
	return(0);
}


int ti_truncate(const char *apath, off_t size){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	if(nd->size == 0) return(0);
	free(nd->data);
	nd->data = (char*)malloc(sizeof(char));
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

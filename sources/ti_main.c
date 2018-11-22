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
#include <math.h>

#define MAXIN 1000
#define MAXDN 10000
#define INDISKSZ 512
#define DNDISKSZ 1024
#define DPERN 10

const int DISKSIZE = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) + (MAXDN * DNDISKSZ);

typedef struct INODE{
	char type;
	off_t size;
	char * data;
	char path[50];
	char name[20];
	uid_t user_id;
	int num_files;
	time_t a_time;
	time_t m_time;
	time_t b_time;
	gid_t group_id;
	int num_children;
	mode_t permissions;
	struct INODE * parent;
	long int children[30];
	long int datab[DPERN];
	unsigned long int i_number;
}INODE;

INODE *ROOT;
int HDISK;
char *IBMAP;
char *DBMAP;
unsigned long int IN = 0;

void openDisk();
void closeDisk();
int delNode(char *);
int getInodeNumber();
int getDnodeNumber();
void clearInode(int);
void clearDnode(int);
char * getDnode(int);
INODE * getInode(int);
char * getDir(char *);
char * getName(char **);
void storeInode(INODE *);
int addNode(char *, char);
char * reverse(char *, int);
int storeDnode(char *, int);
void clearDnodeNumber(int);
void clearInodeNumber(int);
void initializeTIFS(INODE **);
INODE * getNodeFromPath(char *, INODE *);
INODE * initializeNode(char *, char *, char, INODE *);

int ti_rmdir(const char *);
int ti_unlink(const char *);
int ti_link(const char *, const char *);
int ti_access(const char *, int);
int ti_chmod(const char *, mode_t );
int ti_mkdir(const char *, mode_t );
int ti_truncate(const char *, off_t );
int ti_mknod(const char *, mode_t, dev_t);
int ti_getattr(const char *, struct stat *);
int ti_utime(const char *, struct utimbuf *);
int ti_open(const char *, struct fuse_file_info *);
int ti_create(const char *, mode_t , struct fuse_file_info *);
int ti_read(const char *, char *, size_t , off_t ,struct fuse_file_info *);
int ti_write(const char *, const char *, size_t , off_t , struct fuse_file_info *);
int ti_readdir(const char *, void *, fuse_fill_dir_t , off_t , struct fuse_file_info *);

static struct fuse_operations operations = {
	.open       = ti_open,
	.read       = ti_read,
	.link       = ti_link,
	.mkdir      = ti_mkdir,
	.rmdir      = ti_rmdir,
	.mknod      = ti_mknod,
	.write      = ti_write,
	.utime      = ti_utime,
	.chmod      = ti_chmod,
	.create     = ti_create,
	.access     = ti_access,
	.unlink     = ti_unlink,
	.getattr    = ti_getattr,
	.readdir    = ti_readdir,
	.truncate   = ti_truncate,
};

void openDisk(){HDISK = open("metaFiles/HDISK.meta", O_RDWR | O_CREAT , S_IRUSR | S_IWUSR);}
void closeDisk(){close(HDISK);}

int getInodeNumber(){
	int ino;
	openDisk();
	lseek(HDISK, 0, SEEK_SET);
	memset(IBMAP, '0', MAXIN);
	read(HDISK, IBMAP, MAXIN);
	for(ino=0; ino<MAXIN; ino++) if(IBMAP[ino] == '0') break;
	IBMAP[ino] = '1';
	lseek(HDISK, 0, SEEK_SET);
	write(HDISK, IBMAP, MAXIN);
	closeDisk();
	return(ino);
}

void clearInodeNumber(int ino){
	openDisk();
	lseek(HDISK, 0, SEEK_SET);
	memset(IBMAP, '0', MAXIN);
	read(HDISK, IBMAP, MAXIN);
	IBMAP[ino] = '0';
	lseek(HDISK, 0, SEEK_SET);
	write(HDISK, IBMAP, MAXIN);
	closeDisk();
}

int getDnodeNumber(){
	int dno;
	openDisk();
	lseek(HDISK, MAXIN, SEEK_SET);
	read(HDISK, DBMAP, MAXDN);
	for(dno=1; dno<MAXDN; dno++) if(DBMAP[dno] == '0') break;
	DBMAP[dno] = '1';
	lseek(HDISK, MAXIN, SEEK_SET);
	write(HDISK, DBMAP, MAXDN);
	closeDisk();
	return(dno);
}

void clearDnodeNumber(int dno){
	openDisk();
	lseek(HDISK, MAXIN, SEEK_SET);
	read(HDISK, DBMAP, MAXDN);
	DBMAP[dno] = '0';
	lseek(HDISK, MAXIN, SEEK_SET);
	write(HDISK, DBMAP, MAXDN);
	closeDisk();
}


void clearInode(int ino){
	int offset = (MAXIN + MAXDN) + (ino * INDISKSZ);
	char *buff = (char*)malloc(sizeof(char)*INDISKSZ);
	memset(buff, 0, INDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	write(HDISK, buff, INDISKSZ);
	free(buff);
	closeDisk();
}

void clearDnode(int dno){
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) +(dno * DNDISKSZ);
	char *buff = (char*)malloc(sizeof(char)*DNDISKSZ);
	memset(buff, 0, DNDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	write(HDISK, buff, DNDISKSZ);
	free(buff);
	closeDisk();
}

int storeDnode(char *data, int dno){
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) + (dno * DNDISKSZ);
	int wlen = (strlen(data) > DNDISKSZ) ? DNDISKSZ : strlen(data);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	wlen = write(HDISK, data, wlen);
	closeDisk();
	return(wlen);
}

char * getDnode(int dno){
	char *toret = (char*)malloc(sizeof(char)*DNDISKSZ);
	int offset = (MAXIN + MAXDN) + (MAXIN * INDISKSZ) + (dno * DNDISKSZ);
	openDisk();
	lseek(HDISK, offset, SEEK_SET);
	read(HDISK, toret, DNDISKSZ);
	closeDisk();
	return(toret);
}

void storeInode(INODE *nd){
	if(nd == NULL) return;
	int offset = (MAXIN + MAXDN) + (nd->i_number * INDISKSZ);
	off_t bw;
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
		int dno = 0, dsize = 0, i = 0, doff = 0, wlen;
		if(nd->data == NULL) nd->size=0;
		else{
			nd->size = 0; dsize = strlen(nd->data);
			for(dsize = strlen(nd->data), i=0, doff = 0; dsize > 0; dsize -= DNDISKSZ, doff += DNDISKSZ, i++){
				closeDisk();
				clearDnodeNumber(nd->datab[i]);
				clearDnode(nd->datab[i]);
				dno = getDnodeNumber();
				wlen = storeDnode(nd->data + doff, dno);
				nd->datab[i] = dno;
				nd->size += wlen;
				openDisk();
			}
		}
		for(i=i;i<DPERN;i++) {closeDisk();clearDnodeNumber(nd->datab[i]);clearDnode(nd->datab[i]);nd->datab[i]=0;openDisk();}
		lseek(HDISK, bw, SEEK_SET);
		write(HDISK, &(nd->datab), sizeof(nd->datab));
	}
	else write(HDISK, nd->children, sizeof(nd->children));
	write(HDISK, &(nd->size), sizeof(nd->size));
	closeDisk();
}


INODE * getInode(int ino){
	INODE *toret = (INODE *)malloc(sizeof(INODE));
	char *buff = (char*)malloc(sizeof(char)*INDISKSZ);
	openDisk();
	toret->i_number = ino;
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
	closeDisk();
	if(toret->type == 'f'){
		if(toret->size){
			toret->data = (char*)malloc(sizeof(char) * (toret->size)); strcpy(toret->data, "");
			for(int i=0; strlen(toret->data) < toret->size; i++) {strcat(toret->data, getDnode(toret->datab[i]));}
		}
		else toret->data = NULL;
	}
	return(toret);
}

char * reverse(char * str, int mode){
	int len = strlen(str);
	char * retval = (char *)calloc(sizeof(char), (len + 1));
	for(int i = 0; i <= len/2; i++){
		retval[i] = str[len - 1 -i];
		retval[len - i - 1] = str[i];
	}
	if(retval[0] == '/' && mode == 1) retval++;
	return(retval);
}

char * getName(char ** copy_path){
	char * retval = (char *)calloc(sizeof(char), 1);
	int retlen = 0;
	char temp;
	char * tempstr;
	*copy_path = reverse(*copy_path, 1);
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
	if(strlen(*copy_path) > 1) (*copy_path)++;
	retval = (char *)realloc(retval, sizeof(char) * (retlen + 1));
	retval[retlen] = '\0';
	retval = reverse(retval, 0);        
	*(copy_path) = reverse(*(copy_path), 0);
	return(retval);
}

char * getDir(char * apath){
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	if(path == NULL) return(NULL);
	char *dirp = reverse((char*)path, 1);
	while(dirp[0] != '/') dirp++;
	free(path);
	return(reverse(dirp, 0));
}

INODE * getNodeFromPath(char * apath, INODE *parent){
	int i;
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	if((path[strlen(path)-1] == '/') && (strcmp("/", path) != 0)) path[strlen(path) - 1] = '\0';
	INODE *retn, *tmp;
	if((path == NULL)||(parent == NULL)||(strcmp(path, "") == 0)) return(NULL);
	if(strcmp(parent->path, path) == 0) return(parent);
	
	for(i=0; i<parent->num_children; i++){
		tmp = getInode((parent->children)[i]);
		retn = getNodeFromPath(path, tmp);
		if(retn != NULL) {return(retn);}
		free(tmp);
	}
	return(NULL);
}

int delNode(char *apath){
	int i, j, flag = 0, dsize, doff;
	if(apath == NULL) return(0);
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	INODE *parent = getNodeFromPath(getDir(path), ROOT);
	if(parent == NULL) return(0);
	for(i=0; i<parent->num_children; i++){
		INODE *child = getInode(parent->children[i]);
		if(strcmp(child->path, path) == 0){
			if((child->type == 'd') && (child->num_children != 0)) return(-ENOTEMPTY);
			if((child->type == 'f') && (child->data != NULL)){
				for(dsize = strlen(child->data), doff=0, j=0; dsize>0; dsize -= DNDISKSZ, doff += DNDISKSZ, j++){
					clearDnodeNumber(child->datab[j]);
					clearDnode(child->datab[j]);
				}
			}
			clearInodeNumber(child->i_number);
			clearInode(child->i_number);
			flag = 1;
		}
		if(flag) if(i != parent->num_children-1) parent->children[i] = parent->children[i+1];
	}
	parent->num_children -= 1;
	storeInode(parent);
	storeInode(ROOT);
	free(path);
	return(0);
}

int addNode(char * apath, char type){
	if(apath == NULL) return(0);
	char *path = (char*)malloc(sizeof(char)*strlen(apath));
	strcpy(path, apath);
	INODE *parent = getNodeFromPath(getDir(path), ROOT);
	if(parent == NULL) return(0);
	parent->num_children += 1;
	INODE *child = initializeNode(apath, getName(&path), type, parent);
	parent->children[parent->num_children - 1] = child->i_number;
	storeInode(parent);
	storeInode(child);
	storeInode(ROOT);
	free(path);
	return(0);
}

INODE * initializeNode( char *path, char *name, char type, INODE *parent){
	INODE *ret = (INODE*)malloc(sizeof(INODE));
	strcpy(ret->path, path);
	strcpy(ret->name, name);
	ret->type = type;
	if(type == 'd'){ret->permissions = S_IFDIR | 0755;ret->size = INDISKSZ;}     
	else{ret->permissions = S_IFREG | 0644;ret->size = 0;} 
	ret->user_id = getuid();
	ret->group_id = getgid();
	ret->num_children = 0;
	ret->num_files = 0;
	time(&(ret->a_time));
	time(&(ret->m_time));
	time(&(ret->b_time));
	ret->i_number = getInodeNumber();
	ret->parent = parent;
	ret->data = NULL;
	return(ret);
}

void initializeTIFS(INODE **rt){
	IBMAP = (char*)malloc(sizeof(char)*(MAXIN));
	DBMAP = (char*)malloc(sizeof(char)*(MAXDN));
	if(access("metaFiles/HDISK.meta", F_OK ) != -1){}
	else{
		char *buf = (char*)malloc(sizeof(char)*DISKSIZE);
		openDisk();
		memset(buf, '0', DISKSIZE);
		lseek(HDISK, 0, SEEK_SET);
		write(HDISK, buf, MAXIN);
		write(HDISK, buf, MAXDN);
		closeDisk();		
		(*rt) = initializeNode( "/", "TIROOT", 'd',  (*rt));
		storeInode((*rt));
		free(buf);
	}
	(*rt) = getInode(0);
}


int ti_getattr(const char *apath, struct stat *st){
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
	st->st_blocks = ceil((float)nd->size / (float)(DNDISKSZ));
	st->st_blksize = DNDISKSZ;
	return(0);
}


int ti_readdir(const char *apath, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ){
	filler(buffer, ".", NULL, 0 ); 
	filler(buffer, "..", NULL, 0 );
	INODE *nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	nd->a_time=time(NULL);
	for(int i = 0; i < nd->num_children; i++) filler( buffer, getInode(nd->children[i])->name, NULL, 0 );
	return(0);
}


int ti_write(const char *apath, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	if(apath == NULL) return(-ENOENT);
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	nd->m_time = time(NULL);
	nd->a_time = time(NULL);
	nd->size = (nd->size > size + offset) ? nd->size : size + offset;
	if(nd->data == NULL) nd->data = (char *)malloc(sizeof(char)*(nd->size));
	else nd->data = (char *)realloc(nd->data, sizeof(char)*(nd->size));
	memcpy(nd->data + offset, buf, size);
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
	memcpy(buf, nd->data + offset, nd->size - offset);
	storeInode(nd);
	return(size);
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


int ti_chmod(const char *apath, mode_t new){
    if(apath == NULL) return(-ENOENT);
    INODE * nd = getNodeFromPath((char *) apath, ROOT);
    if(nd == NULL) return(-ENOENT);
    nd->m_time = time(NULL);
    nd->permissions = new;
    storeInode(nd);
    return(0);
}


int ti_link(const char *oldp, const char *newp){
    INODE *old = getNodeFromPath((char *) oldp, ROOT);
    if(old == NULL) return(-ENOENT);
    addNode((char*) newp, old->type);
    INODE *new = getNodeFromPath((char *) newp, ROOT);
    old->num_children += 1;
    new->data = (char *)malloc(sizeof(char)*(strlen(old->data)+1));
    strcpy(new->data, old->data);
    storeInode(old);
    storeInode(new);
    return(0);
}


int ti_open(const char *apath, struct fuse_file_info *fi){
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	if(nd == NULL) return(-ENOENT);
	nd->a_time = time(NULL);
	storeInode(nd);
	return(0);
}


int ti_utime(const char *apath, struct utimbuf *tv){
	INODE * nd = getNodeFromPath((char *) apath, ROOT);
	nd->m_time = time(NULL);
	nd->a_time = time(NULL);
	storeInode(nd);
	return(0);
}


int ti_access(const char * apath, int mask){
	int grant = 1;		
	if(grant) return(0);
	return(-EACCES);
}


int ti_rmdir(const char * apath){return(delNode((char *) apath));}
int ti_unlink(const char *apath){return(delNode((char *) apath));}
int ti_mkdir(const char * apath, mode_t x){return(addNode((char*) apath, 'd'));}
int ti_mknod(const char * apath, mode_t x, dev_t y){return(addNode((char*) apath, 'f'));}
int ti_create(const char *apath, mode_t mode, struct fuse_file_info *fi){return(addNode((char*) apath, 'f'));}

int main(int argc, char *argv[]){initializeTIFS(&ROOT);return fuse_main(argc, argv, &operations);}

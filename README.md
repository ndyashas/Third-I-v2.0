# Third-I-v2.0
simple implementation of file system on Linux using FUSE. 

## Usage

* To download the code and start the file system
```
$ git clone https://github.com/yashasbharadwaj111/Third-I-v2.0
$ cd Third-I-v2.0
```
* For debug run ``` $ make debug ``` , for normal run ```$ make ```.


* Now the file system is mounted at ```Third-I-v2.o/mountPoint/``` you can go into the directory and check for some of the FS functions. 

* No graceful method (yet) is used to unmount the FS at mountPoint
thus , do the following to unmount the FS. Thus,  use
-> ``` $ make stop``` to unmount TIFS.
-> ``` $ make format``` to unmount and format TIFS.

* Modifications can be easily done in the [sources/ti_main.c](https://github.com/yashasbharadwaj111/Third-I-v2.0/blob/master/sources/ti_main.c) file .
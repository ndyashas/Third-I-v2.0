# Third-I-v2.0
simple implementation of file system for Linux.

## Usage

* To download the code and start the file system
```
$ git clone https://github.com/yashasbharadwaj111/Third-I-v2.0
$ cd Third-I-v2.0
$ make
```

* Now the file system is mounted at ```Third-I-v2.o/mountPoint/``` you can go into the directory and check for some of the FS functions. try ```stat .``` which will give an output that clearly says which state our FS is in.

* No graceful method is now used to unmount the FS at mountPoint
thus , do the following to unmount the FS
```
$ cd Third-I
$ make stop
```

* To format the file system
```
$ make format
```
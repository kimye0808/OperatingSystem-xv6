struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};
/*FD_PIPE: 이 값은 struct file이 파이프를 나타내는 파일임을 나타냅니다. 파이프는 프로세스 간 통신을 위한 메커니즘으로 사용되는 특수한 유형의 파일입니다. 파이프는 읽기와 쓰기 모두 가능한 양방향 통신 채널로 사용될 수 있습니다.
FD_INODE: 이 값은 struct file이 디스크 상의 파일을 나타내는 파일임을 나타냅니다. 파일 시스템의 일부로서 파일은 inode (index node)라고 불리는 메타데이터 구조와 관련됩니다. FD_INODE는 이러한 디스크 파일을 가리키는 struct inode에 대한 포인터를 가지고 있습니다.*/

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1+2];// for project03 file multi indirect
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

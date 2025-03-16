#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

/*xv6의 `log.c` 파일에 있는 `log_write()` 함수는 그룹 플러시(group flush)를 수행하는 과정을 일부 포함하고 있습니다. 그룹 플러시는 로그에 있는 여러 수정된 블록들을 한 번에 디스크에 기록하는 작업입니다. `log_write()` 함수는 다음과 같은 단계를 거쳐 그룹 플러시를 수행합니다:

1. `log_write()` 함수는 로그 헤더(`log.lh`)에 블록 번호(`b->blockno`)를 기록하기 전에 해당 블록 번호가 이미 로그에 포함되어 있는지 검사합니다. 로그 헤더를 순회하며 이미 포함되어 있다면 추가적인 처리를 수행하지 않고 종료합니다. 이는 중복된 블록 번호를 로그에 중복해서 기록하지 않기 위한 검사입니다.

2. 만약 로그에 새로운 블록 번호라면, 로그 헤더(`log.lh`)에 해당 블록 번호를 추가합니다. 이를 위해 로그 헤더의 블록 번호 배열에 새로운 블록 번호를 기록하고, 로그 헤더의 블록 번호 개수(`log.lh.n`)를 증가시킵니다.

3. 버퍼(`struct buf`)의 플래그(`b->flags`)에 `B_DIRTY` 플래그를 설정하여 버퍼가 삭제되지 않도록 합니다. 이는 버퍼가 로그에 의해 사용 중이라는 것을 나타냅니다. `B_DIRTY` 플래그가 설정되면, 해당 버퍼는 LRU 캐시에서 제거되지 않고 유지됩니다.

그룹 플러시는 `commit()` 함수에서 호출되며, 해당 함수에서는 `write_log()`를 통해 수정된 버퍼들을 로그에서 디스크로 기록하고, `install_trans()`를 통해 로그에서 디스크로 변경된 블록들을 복사합니다. 그룹 플러시는 트랜잭션의 변경 내용을 한 번에 디스크로 영구적으로 반영하는 역할을 수행합니다.*/



/*
이 파일은 xv6 파일 시스템의 로깅 시스템을 구현하는 log.c입니다. 로깅 시스템은 여러 개의 파일 시스템 시스템 호출이 동시에 발생할 수 있는 환경에서 안전하게 파일 시스템 변경 작업을 수행하기 위한 기능을 제공합니다.

다음은 주요 함수들에 대한 설명입니다:

initlog(int dev): 로깅 시스템을 초기화합니다. 주어진 장치(dev)를 기반으로 로그의 시작 위치, 크기 등을 설정하고 recover_from_log() 함수를 호출하여 이전에 로그에 저장된 변경 내용을 복구합니다.

install_trans(): 로그에서 커밋된 블록들을 해당 위치로 복사하여 디스크에 저장합니다. 커밋된 블록은 로그 블록과 대응되는 목적지 디스크 블록으로 복사됩니다.

read_head(): 디스크에서 로그 헤더를 읽어와 메모리 내의 로그 헤더에 저장합니다. 이 함수는 로그 헤더의 내용을 메모리에 유지하고 로그 작업을 진행할 때 참조할 수 있도록 합니다.

write_head(): 메모리 내의 로그 헤더를 디스크에 저장합니다. 이 함수는 현재 트랜잭션의 변경 내용을 커밋하는 시점으로, 로그의 실제 커밋이 이루어집니다.

recover_from_log(): 로그를 복구합니다. read_head()를 통해 디스크에서 로그 헤더를 읽어와 메모리에 저장하고, install_trans()를 호출하여 로그에 저장된 변경 내용을 디스크로 복사한 후 로그를 초기화합니다.

begin_op(): 각 파일 시스템 시스템 호출의 시작 시점에 호출됩니다. 현재 진행 중인 FS 시스템 호출의 수를 추적하고, 로그 공간이 부족해지면 로그가 커밋될 때까지 대기합니다.

end_op(): 각 파일 시스템 시스템 호출의 종료 시점에 호출됩니다. 현재 진행 중인 FS 시스템 호출의 수를 감소시키고, 만약 마지막 진행 중인 호출이라면 커밋을 수행합니다.

write_log(): 캐시에서 수정된 블록들을 로그로 복사합니다. 로그 블록과 대응되는 캐시 블록의 데이터를 복사하고 로그에 기록합니다.

commit(): 로그에서 변경된 블록들을 디스크로 기록하고, 변경 내용을 해당 위치로 설치합니다. 로그의 내용을 지우고 커밋을 완료합니다.

log_write(struct buf *b): 수정된 버퍼를 로그에 기록합니다. 버퍼의 블록 번호를 로그 헤더에 추가하고, 버퍼의 B_DIRTY 플래그를 설정하여 버퍼가 로그에 사용 중임을 표시합니다.

이러한 함수들을 조합하여 xv6 파일 시스템은 여러 개의 파일 시스템 시스템 호출을 안전하게 처리할 수 있게 됩니다. 로그를 통해 변경 내용이 안정적으로 디스크에 반영되고, 중복된 로그 기록이나 충돌을 방지합니다.*/


// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    //} else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
    }else if(log.lh.n >= LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    if(log.lh.n + MAXOPBLOCKS > LOGSIZE){/////for project03
      do_commit = 1;
      log.committing = 1;
    }
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}



// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}


//for project03, sync function
int 
sync(void)
{
  int flushNum = LOGSIZE;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1){
    release(&log.lock);
    return -1;
  }
  flushNum = log.lh.n;
  do{
    if(log.committing || log.outstanding){
      sleep(&log, &log.lock);
    }
    log.committing = 1;
    release(&log.lock);
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
  }while(log.committing || log.outstanding);
  release(&log.lock);

  return flushNum;
}


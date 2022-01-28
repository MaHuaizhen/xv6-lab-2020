// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include "proc.h"
int T_noff = 0;
#define BUCKET_NO 13
struct {
  struct spinlock lock;
  //struct buf buf[NBUF];//需要由time-stamp为参考的列表替换
  bucket bucket[BUCKET_NO];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  uint8 key;

} bcache;
uint8 getHashBucket(uint blockno)
{
  return blockno%BUCKET_NO;
}
struct buf* getLeastUsedBUf(uint8 key);
struct buf* getLeastUsedBUf(uint8 key)
{
  struct buf* b ;
  struct buf* ret_b = 0;
  uint deltTime = 0;
  //printf("bget:key:%d\n",key);
  for(b =bcache.bucket[key].buf;b<&bcache.bucket[key].buf[10];b++)
  {
    if(b->time_stamp <= ticks)
    {
      //printf("ticks not out\n");
      if(((ticks - b->time_stamp) >= deltTime)&&(b->refcnt == 0))
      {
        ret_b = b;
        deltTime = ticks - b->time_stamp;
        //printf("find block:deltTime:%d\n",deltTime);
        
      }
    }
    else
    {
      //printf("ticks out?ticks:%d,b-time_stamp:%d\n",ticks,b->time_stamp);
    }
  }
  //printf("find block:block timestamp:%d,ticks:%d,\n",ret_b->time_stamp,ticks);
  return ret_b;
}
// char* bucketname[13] = {"bcache_bucket0","bcache_bucket1","bcache_bucket2","bcache_bucket3","bcache_bucket4","bcache_bucket5","bcache_bucket6","bcache_bucket7","bcache_bucket0"};
void
binit(void)
{
  //struct buf *b;

  initlock(&bcache.lock, "bcache");
  //初始化bucket所有的锁
  for(int i = 0; i<BUCKET_NO;i++)
  {
    initlock(&bcache.bucket[i].lock,"bcache_bucket");
    for(int j=0;j<10;j++)
    {
      //初始化bucket内buf的所有的锁
      initsleeplock(&bcache.bucket[i].buf[j].lock,"buffer");
    }
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{

  
  struct buf *b;
  //printf("bget:blockn0:%d,dev:%d\n",blockno,dev);
  //int i = 0;
  //acquire(&bcache.lock);

  // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  //bcache.key = getHashBucket(blockno);
  acquire(&bcache.bucket[getHashBucket(blockno)].lock);
  
  for(b =bcache.bucket[getHashBucket(blockno)].buf;b<&bcache.bucket[getHashBucket(blockno)].buf[10];b++)
  {
    if(b->dev ==dev&&b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.bucket[getHashBucket(blockno)].lock);
    //printf("get lockblock:%d proc:%d\n",b->blockno,myproc()->pid);
     acquiresleep(&b->lock); 
     return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  //根据time-stamp选取最不经常使用的block cache
  b = getLeastUsedBUf(getHashBucket(blockno));
  if(b)
  {
    //b->time_stamp = ticks;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->dev = dev;
    
    release(&bcache.bucket[getHashBucket(blockno)].lock);
   //printf("get lockblock:%d proc:%d\n",b->blockno,myproc()->pid);
    acquiresleep(&b->lock); 
    return b;  
  }


  // for(b =bcache.bucket[bcache.key].buf;b<&bcache.bucket[bcache.key].buf[NBUF];b++)
  // {
  //   if(b->refcnt == 0)
  //   {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     //b->time_stamp = ticks;
  //     acquiresleep(&b->lock); 
  //     return b;
  //   }
  //}
  panic("bget: no buffers");  //此处可以回收不常用的block
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
  T_noff = mycpu()->noff;
  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  //printf("release lock ---->block:%d proc:%d\n",b->blockno,myproc()->pid);
  if(!holdingsleep(&b->lock))
  // {
  //   printf("enter endless loop\n");
  //   while(1);
  // }
  {
     panic("brelse");
  }
 
  releasesleep(&b->lock);

  //acquire(&bcache.lock);
  b->refcnt--;
  if(b->refcnt == 0)
  {
    //printf("set time stamp:blockno %d\n",b->blockno);
    b->time_stamp = ticks;
  }
  
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;//将b从原本位置去除,插入到head后面
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;//将b擦入到buf的最前面
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucket[getHashBucket(b->blockno)].lock);
  b->refcnt++;
  release(&bcache.bucket[getHashBucket(b->blockno)].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket[getHashBucket(b->blockno)].lock);
  b->refcnt--;
  release(&bcache.bucket[getHashBucket(b->blockno)].lock);
}



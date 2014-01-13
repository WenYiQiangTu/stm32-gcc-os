#include <rtthread.h>
#include <dfs_posix.h>
#include "board.h"
#include "playerbuf.h"
#include "vs1003.h"

/* No banked memory. */
#define NutSegBufEnable(bank)

#define NutHeapAlloc(siz)

static char segbuf_empty;
static int segbuf_total;
static int segbuf_used;

static char *segbuf_start;
static char *segbuf_end;


static char *segbuf_wp;
static char segbuf_ws;
static char *segbuf_rp;
static char segbuf_rs;

// 1K
static char NutSefBufHeap[1024 * 1];

char *NutSegBufReset(void)
{
    segbuf_rp = segbuf_wp = segbuf_start;
    segbuf_rs = segbuf_ws = 0;
    NutSegBufEnable(0);
    segbuf_empty = 1;
    segbuf_used = 0;

    return segbuf_start;
}

char *NutSegBufInit(size_t size)
{

#if NUTBANK_COUNT
    segbuf_start = (char *)(NUTBANK_START);
    segbuf_end = (char *)(NUTBANK_START) + NUTBANK_SIZE;
    segbuf_total = (uint32_t) NUTBANK_COUNT *(uint32_t) NUTBANK_SIZE;
#else
    /*
	if (size == 0)
        size = NutHeapAvailable() / 2;
    if (segbuf_start) {
        NutHeapFree(segbuf_start);
    }
	*/
    segbuf_start = &NutSefBufHeap[0];
    segbuf_end = &NutSefBufHeap[1024];
    //if ((segbuf_start = NutHeapAlloc(size)) != NULL)
    //    segbuf_end = segbuf_start + size;
    segbuf_total = 1024;
#endif

    return NutSegBufReset();
}

char *NutSegBufWriteRequest(size_t * bcp)
{
    if (segbuf_empty || segbuf_ws != segbuf_rs || segbuf_wp > segbuf_rp)
        *bcp = segbuf_end - segbuf_wp;
    else
        *bcp = segbuf_rp - segbuf_wp;

    NutSegBufEnable(segbuf_ws);
    return segbuf_wp;
}


char *NutSegBufReadRequest(size_t * bcp)
{
    if (segbuf_empty)
        *bcp = 0;
    else if (segbuf_ws != segbuf_rs || segbuf_rp >= segbuf_wp)
        *bcp = segbuf_end - segbuf_rp;
    else if ((*bcp = segbuf_wp - segbuf_rp) == 0 && segbuf_ws == segbuf_rs)
        segbuf_empty = 1;

    NutSegBufEnable(segbuf_rs);
    return segbuf_rp;
}

char *NutSegBufWriteCommit(size_t bc)
{
    if (bc) {
        segbuf_wp += bc;
        segbuf_empty = 0;
        segbuf_used += bc;
        if (segbuf_wp == segbuf_end) {
            segbuf_wp = segbuf_start;
#if NUTBANK_COUNT > 0
            if (++segbuf_ws >= NUTBANK_COUNT)
                segbuf_ws = 0;
#endif
            NutSegBufEnable(segbuf_ws);
        }
    }
    return segbuf_wp;
}

char *NutSegBufReadCommit(size_t bc)
{
    if (bc) {
        segbuf_rp += bc;
        segbuf_used -= bc;
        if (segbuf_rp == segbuf_end) {
            segbuf_rp = segbuf_start;
#if NUTBANK_COUNT > 0
            if (++segbuf_rs >= NUTBANK_COUNT)
                segbuf_rs = 0;
#endif
            NutSegBufEnable(segbuf_rs);
        }
        if (segbuf_rp == segbuf_wp  && segbuf_rs == segbuf_ws)
            segbuf_empty = 1;
    }
    return segbuf_rp;
}

/*!
 * \brief Commit written buffer space and finish write access.
 *
 * The write pointer will be incremented by the specified number of bytes.
 * This call will also enable the current read segment and may disable the 
 * current write segment.
 *
 * \param bc Number of bytes to commit.
 */
void NutSegBufWriteLast(size_t bc)
{
    if (bc) {
        segbuf_wp += bc;
        segbuf_used += bc;
        segbuf_empty = 0;
        if (segbuf_wp == segbuf_end) {
            segbuf_wp = segbuf_start;
#if NUTBANK_COUNT > 0
            if (++segbuf_ws >= NUTBANK_COUNT)
                segbuf_ws = 0;
#endif
        }
    }
    NutSegBufEnable(segbuf_rs);
}

void NutSegBufReadLast(size_t bc)
{
    if (bc) {
        segbuf_rp += bc;
        segbuf_used -= bc;
        if (segbuf_rp == segbuf_end) {
            segbuf_rp = segbuf_start;
#if NUTBANK_COUNT > 0
            if (++segbuf_rs >= NUTBANK_COUNT)
                segbuf_rs = 0;
#endif
        }
        if (segbuf_rp == segbuf_wp && segbuf_rs == segbuf_ws)
            segbuf_empty = 1;
    }
    NutSegBufEnable(segbuf_ws);
}

/*!
 * \brief Return the available buffer space.
 *
 * \return Total number of free bytes in the buffer.
 */
uint32_t NutSegBufAvailable(void)
{
    return segbuf_total - segbuf_used;
}

/*!
 * \brief Return the used buffer space.
 *
 * \return Total number of used bytes in the buffer.
 */
uint32_t NutSegBufUsed(void)
{
    return segbuf_used;
}

void mp3_player_thread(char *filename)
{
    int fd;
    unsigned int rbytes = 0, got = 0;
    char *mp3buf = RT_NULL;
    
    if (!filename) return ;

    NutSegBufInit(1024);
    fd = open(filename, O_RDONLY, 0);
    for (;;) 
    {
        /*
         * Query number of byte available in MP3 buffer.
         */
        mp3buf = NutSegBufWriteRequest(&rbytes);
        /* 
         * Read data directly into the MP3 buffer. 
         */
        if (rbytes) 
        {
            rt_kprintf("[B.RD%d]", rbytes);
            if ((got = read(fd, mp3buf, rbytes)) > 0) 
            {
                rt_kprintf("[B.CMT%d]", got);
                mp3buf = NutSegBufWriteCommit(got);

//                VsPlayerFeed(RT_NULL);
            }
            else 
            {
                rt_kprintf("[EOF]");
                break;
            }

            continue;
        }
        /*
         * If the player is not running, kick it.
         */
        if (VsGetStatus() != VS_STATUS_RUNNING) 
        {
            rt_kprintf("[P.KICK]");
            VsPlayerKick();
        }
        rt_thread_delay(10);

    }
    close(fd);
    mp3buf = NutSegBufReset();
}

#ifdef RT_USING_FINSH
#include <finsh.h>
void mp3() 
{
    mp3_player_thread("test.mp3");
}
FINSH_FUNCTION_EXPORT(mp3, player test);
#endif

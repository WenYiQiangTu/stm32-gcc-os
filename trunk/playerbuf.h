/*
 * =====================================================================================
 *
 *       Filename:  playerbuf.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014/1/10 17:12:48
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ffyy (), 
 *        Company:  
 *
 * =====================================================================================
 */

#ifndef  __playerbuf_h___INC
#define  __playerbuf_h___INC

typedef unsigned int size_t;

/* Prototypes */
extern char *NutSegBufReset(void);
extern char *NutSegBufInit(size_t size);
extern char *NutSegBufWriteRequest(size_t * bcp);
extern char *NutSegBufReadRequest(size_t * bcp);
extern char *NutSegBufWriteCommit(size_t bc);
extern char *NutSegBufReadCommit(size_t bc);
extern void NutSegBufWriteLast(size_t bc);
extern void NutSegBufReadLast(size_t bc);
extern uint32_t NutSegBufAvailable(void);
extern uint32_t NutSegBufUsed(void);


#endif   /* ----- #ifndef __playerbuf_h___INC  ----- */

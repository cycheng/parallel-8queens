#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <memory.h>
#include <assert.h>
#include "3rd-party\OgrePlatform.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#   include "3rd-party\OgreTimerWin32.h"
#else
#   pragma error "Not implement yet !"
#endif

typedef struct _chessboard_status_t {
    unsigned int size;       // size = n
    unsigned int curRow;
    uint32 all;
} chessboard_status_t;

typedef struct _worker_t {
    pthread_mutex_t lock;
    pthread_cond_t condGo;
    pthread_cond_t condDone;
    pthread_t thread;
    int stop;
    int done;
    chessboard_status_t *board;
    unsigned int numSol;

    // shared variables between threads, we don't use lock/unlock to protect
    // them, instead we use atomic function
    unsigned int *colToBeProcessed;
    unsigned int *totalSolution;

    unsigned int *totalForLoopCount;

    unsigned int *runCount;
} worker_t;

void *queenWorker(void *data);

worker_t * createThreadPool(const int numcpu, unsigned int *sharedRunCount);
void destroyThreadPool(worker_t *workerpool, const int numcpu);
int initWorkerThread(worker_t *worker);
void uninitWorkerThread(worker_t *worker);

chessboard_status_t *createChessboardStatePool(const int numboard, const int boardsize);
void destroyChessboardStatePool(chessboard_status_t *boardpool, const int numboard);
int initChessboardState(chessboard_status_t *board, const int size);
void resetChessboardState(chessboard_status_t *board);
void uninitChessboardState(chessboard_status_t *board);

worker_t * createThreadPool(const int numcpu, unsigned int *sharedRunCount) {
    int i = 0;

    worker_t *workerpool = (worker_t *)malloc(sizeof(worker_t) * numcpu);
    if (! workerpool) {
        printf("Create workpool failed, insufficient memory\n");
        goto FAILED;
    }

    memset(workerpool, 0, sizeof(worker_t) * numcpu);

    for (i = 0; i < numcpu; i++) {
        int status;
        worker_t *worker = &workerpool[i];
        worker->runCount = sharedRunCount;
        status = initWorkerThread(worker);
        if (status != 0) {
            printf("Create thread %d failed with error code %d\n", i, status);
            goto FAILED;
        }
    }
    return workerpool;
FAILED :
    destroyThreadPool(workerpool, i);
    return NULL;
}

void destroyThreadPool(worker_t *workerpool, const int numcpu) {
    int i;
    if (! workerpool)
        return;

    for (i = 0; i < numcpu; i++) {
        worker_t *worker = &workerpool[i];
        uninitWorkerThread(worker);
    }
    free(workerpool);
}

int initWorkerThread(worker_t *worker) {
    worker->stop = 0;
    pthread_mutex_init(&worker->lock, NULL);
    pthread_cond_init(&worker->condGo, NULL);
    pthread_cond_init(&worker->condDone, NULL);
    return pthread_create(&worker->thread, NULL, queenWorker, worker);
}

void uninitWorkerThread(worker_t *worker) {
    pthread_mutex_lock(&worker->lock);
    worker->stop = 1;
    pthread_cond_signal(&worker->condGo);
    pthread_mutex_unlock(&worker->lock);

    pthread_join(worker->thread, NULL);
    pthread_cond_destroy(&worker->condGo);
    pthread_cond_destroy(&worker->condDone);
}

chessboard_status_t *createChessboardStatePool(const int numboard, const int boardsize) {
    chessboard_status_t *boards = (chessboard_status_t *)malloc(sizeof(chessboard_status_t) * numboard);
    int i = 0;
    if (! boards)
        goto FAILED;

    memset(boards, 0, sizeof(chessboard_status_t) * numboard);
    for (i = 0; i < numboard; i++)
        if (0 != initChessboardState(&boards[i], boardsize))
            goto FAILED;
    return boards;
FAILED :
    destroyChessboardStatePool(boards, i);
    free(boards);
    return NULL;
}

void destroyChessboardStatePool(chessboard_status_t *boardpool, const int numboard) {
    int i;
    if (! boardpool)
        return;

    for (i = 0; i < numboard; i++) {
        uninitChessboardState(&boardpool[i]);
    }
    free(boardpool);
}

int initChessboardState(chessboard_status_t *board, const int size) {
    board->size = size;
    board->all = (1 << size) - 1;
    resetChessboardState(board);
    return 0;
}

void resetChessboardState(chessboard_status_t *board) {}
void uninitChessboardState(chessboard_status_t *board) {}

/** Algorithm comes from : http://www.cl.cam.ac.uk/~mr10/backtrk.pdf
 *  Reduced unnecessary search space
 *
 *  CY : This algorithm is really beautiful and efficiency !
 */
void queenDFS(worker_t *worker, uint32 leftD, uint32 cols, uint32 rightD, uint32 all) {
    uint32 poss = ~(leftD | cols | rightD) & all;

    if (cols == all) {
        worker->numSol++;
        return;
    }

    while (poss) {
        uint32 bits = poss & (~poss + 1);
        poss -= bits;
        queenDFS(worker, (leftD | bits) << 1, cols | bits, (rightD | bits) >> 1, all);
        //printf("%x, %x, %x\n", (leftD | bits) << 1, cols | bits, (rightD | bits) >> 1);
        //InterlockedIncrement(worker->totalForLoopCount);
    }
}

void *queenWorker(void *data) {
    worker_t *worker = (worker_t *)data;
    unsigned int threadId = InterlockedIncrement(worker->runCount) - 1;

    pthread_mutex_lock(&worker->lock);
    worker->done = 1;
    pthread_cond_signal(&worker->condDone);
    while (1) {
        if (worker->stop) {
            pthread_mutex_unlock(&worker->lock);
            break;
        }

        pthread_cond_wait(&worker->condGo, &worker->lock);

        while (1) {
            // Note! InterlockedIncrement() require the input be aligned on
            // **32 bits boundary**
            // ref : http://msdn.microsoft.com/en-us/library/windows/desktop/ms683614(v=vs.85).aspx
            unsigned int curCol = InterlockedIncrement(worker->colToBeProcessed) - 1;

            if (curCol >= worker->board->size) {
                pthread_cond_signal(&worker->condDone);
                break;
            }

            worker->board->curRow = 1;

            //queenDFS(worker, worker->board->size, worker->board->curRow);
            //queenDFS(worker, 1 << (curCol + 1), 1 << curCol, (1 << curCol) >> 1, worker->board->all);
            //printf("%x, %x, %x\n", 1 << (curCol + 1), curCol | (1 << curCol), 1 >> (curCol + 1));
            queenDFS(worker, 0, 0, 0, worker->board->all);

            InterlockedExchangeAdd(worker->totalSolution, worker->numSol);
            //printf("tid %d : curCol = %d, sol = %d\n", threadId, curCol, worker->numSol);

            worker->numSol = 0;
            //resetChessboardState(worker->board);
            break;
        }
    }
    return NULL;
}

int main(int c, char **v)
{
	int nn;
    OgreTimer_t timer;
    unsigned long endTime = 0;

    if (c <= 1 || (nn = atoi(v[1])) <= 0) nn = 14;

    OgreTimerInit(&timer);

    {
        // variables to be shared & updated **between threads**
        unsigned int totalSolution = 0;
        unsigned int colToBeProcessed = 0;
        unsigned int runCount = 0;
        unsigned int totalForLoopCount = 0;
        // other local variables
        int i;
        /* todo : according to # of real core on the machine */
        const int numcpu = 1;
        worker_t *workerpool = createThreadPool(numcpu, &runCount);
        chessboard_status_t *boardpool = createChessboardStatePool(numcpu, nn);

        if (! workerpool || ! boardpool) {
            printf("Create workpool or boardpool failed, exit test !\n");
            goto EXIT;
        }

        // wait to make sure all threads are ready
        for (i = 0; i < numcpu; i++) {
            worker_t *worker = &workerpool[i];
            pthread_mutex_lock(&worker->lock);
            if (! worker->done) {
                pthread_cond_wait(&worker->condDone, &worker->lock);
            }
            pthread_mutex_unlock(&worker->lock);
        }

        for (i = 0; i < numcpu; i++) {
            chessboard_status_t *board = &boardpool[i];
            worker_t *worker = &workerpool[i];

            worker->totalSolution = &totalSolution;
            worker->colToBeProcessed = &colToBeProcessed;
            worker->totalForLoopCount = &totalForLoopCount;

            worker->board = board;

            pthread_mutex_lock(&worker->lock);
            pthread_cond_signal(&worker->condGo);
            pthread_mutex_unlock(&worker->lock);
        }

        // wait to make sure all threads are done
        for (i = 0; i < numcpu; i++) {
            worker_t *worker = &workerpool[i];
            pthread_mutex_lock(&worker->lock);
            if (! worker->done) {
                pthread_cond_wait(&worker->condDone, &worker->lock);
            }
            pthread_mutex_unlock(&worker->lock);
        }

    EXIT :
        destroyThreadPool(workerpool, numcpu);
        destroyChessboardStatePool(boardpool, numcpu);
        endTime = OgreTimerGetMicroseconds(&timer);
        printf("\nTotal number of solutions : %d \n\n", totalSolution);
	    printf("referenced execution time : %lf msec\n", (float)endTime / 1000.f);
        printf("Total For Loop = %d\n", totalForLoopCount);
	}
    return 0;
}



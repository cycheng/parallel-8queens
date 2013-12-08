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

//Timing function!!!
static unsigned cyc_hi = 0;
static unsigned cyc_lo = 0;

#if OGRE_CPU == OGRE_CPU_X86
void access_counter(unsigned *hi, unsigned *lo)
{
#   if OGRE_COMPILER == OGRE_COMPILER_MSVC
    unsigned int _hi, _lo;
	__asm rdtsc
	__asm mov _lo, eax
    __asm mov _hi, edx

    *hi = _hi;
    *lo = _lo;
#   else
  /* Get cycle counter */
  asm("rdtsc; movl %%edx,%0; movl %%eax,%1"
      : "=r" (*hi), "=r" (*lo)
      : /* No input */
      : "%edx", "%eax");
#   endif
}
#endif

void start_counter()
{
  /* Get current value of cycle counter */
  access_counter(&cyc_hi, &cyc_lo);
}


double get_counter()
{
  unsigned ncyc_hi, ncyc_lo;
  unsigned hi, lo, borrow;
  /* Get cycle counter */
  access_counter(&ncyc_hi, &ncyc_lo);
  /* Do double precision subtraction */
  lo = ncyc_lo - cyc_lo;
  borrow = lo > ncyc_lo;
  hi = ncyc_hi - cyc_hi - borrow;
  return (double) hi * (1 << 30) * 4 + lo;
}

typedef int bit_field;

typedef struct _chessboard_status_t {
    unsigned int size;       // size = n
    int *column;    // n
    int *right;     // 2n - 1
    int *left;      // 2n - 1
    char *chessboard;// nxn
    unsigned int curRow;
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

    unsigned int *runCount;

    // The following mechanism is used to notify main thread that a worker is
    // finished, and the main thread will check if all works are done
    //unsigned int *runCount;     // how many workers are still working ?
    //pthread_mutex_t *doneLock;
    //pthread_cond_t *condDone;
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

void markQueen(chessboard_status_t *board, const int row, const int col) {
    const int size = board->size;
    const int rightidx = col - row + size - 1;
    const int leftidx = col + row;
    char *grid = &board->chessboard[row * size + col];

    assert(board->column[col] && board->right[rightidx] &&
           board->left[leftidx] &&
           *grid == 'x');

    board->column[col] = board->right[rightidx] = board->left[leftidx] = 0;
    *grid = 'Q';
}

void printChessboard(const chessboard_status_t *board) {
    int i, j;
    const int size = board->size;

    printf("\n");
    for(i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            printf("%c", board->chessboard[i*size + j]);
        }
        printf("\n");
    }
}

void queenDFS(worker_t *worker, const int n, const int curRow);

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
        //destroyChessboardState(&worker->board);
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
    board->column = (int *)malloc(sizeof(int) * size);
    board->right = (int *)malloc(sizeof(int) * (2 * size - 1));
    board->left = (int *)malloc(sizeof(int) * (2 * size - 1));
    board->chessboard = (char *)malloc(sizeof(char) * size * size);

    if (! board->column || ! board->right ||
        ! board->left || ! board->chessboard)
    {
        printf("Init chessboard state failed, insufficient memory\n");
        goto FAILED;
    }

    resetChessboardState(board);

    return 0;
FAILED :
    uninitChessboardState(board);
    return -1;
}

void resetChessboardState(chessboard_status_t *board) {
    int i;
    const int size = board->size;
    memset(board->chessboard, 'x', sizeof(char) * size * size);
    for (i = 0; i < 2 * size - 1; i++) {
        board->right[i] = 1;
        board->left[i] = 1;
    }

    for (i = 0; i < size; i++) {
        board->column[i] = 1;
    }
}

void uninitChessboardState(chessboard_status_t *board) {
    if (board->column) free(board->column);
    if (board->right) free(board->right);
    if (board->left) free(board->left);
    if (board->chessboard) free(board->chessboard);
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
            markQueen(worker->board, 0, curCol);

            queenDFS(worker, worker->board->size, worker->board->curRow);

            InterlockedExchangeAdd(worker->totalSolution, worker->numSol);
            //printf("tid %d : curCol = %d, sol = %d\n", threadId, curCol, worker->numSol);

            worker->numSol = 0;
            resetChessboardState(worker->board);
        }
    }
    return NULL;
}

void queenDFS(worker_t *worker, const int n, const int curRow)
{
    chessboard_status_t *board = worker->board;
    if (curRow < n)
    {
        int i;
        for (i = 0; i < n; i++)
        {
            int j = i - curRow + n - 1;
            int k = i + curRow;
			//left and right are arrays for special process
            if (board->column[i] && board->right[j] && board->left[k])
            {
                //Mark Queens and recursive
                char *grid = &board->chessboard[curRow * n + i];
                //board->column[i] = board->right[j] = board->left[k] = 0;
                //*grid = 'Q';
                markQueen(board, curRow, i);

                queenDFS(worker, n, curRow + 1);

                board->column[i] = board->right[j] = board->left[k] = 1;
                *grid = 'x';
            }
        }
    }
    else
    {
        //printChessboard(board);
        worker->numSol++;
    }
}

int main(int c, char **v)
{
	int nn;
	//double executionTime;
    OgreTimer_t timer;
    unsigned long endTime = 0;

    if (c <= 1 || (nn = atoi(v[1])) <= 0) nn = 8;

    OgreTimerInit(&timer);
    //scanf("%d", &n);
    //start_counter();

    {
        // variables to be shared & updated **between threads**
        unsigned int totalSolution = 0;
        unsigned int colToBeProcessed = 0;
        unsigned int runCount = 0;
        //unsigned int runCount = 0;
        //pthread_mutex_t doneLock = PTHREAD_MUTEX_INITIALIZER;
        //pthread_cond_t condDone = PTHREAD_COND_INITIALIZER;

        // other local variables
        int i;
        /* todo : according to # of real core on the machine */
        const int numcpu = 8;
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

        //pthread_mutex_lock(&doneLock);
        //runCount = numcpu;
        for (i = 0; i < numcpu; i++) {
            chessboard_status_t *board = &boardpool[i];
            worker_t *worker = &workerpool[i];

            worker->totalSolution = &totalSolution;
            worker->colToBeProcessed = &colToBeProcessed;

            //worker->runCount = &runCount;
            //worker->doneLock = &doneLock;
            //worker->condDone = &condDone;

            worker->board = board;

            pthread_mutex_lock(&worker->lock);
            pthread_cond_signal(&worker->condGo);
            pthread_mutex_unlock(&worker->lock);
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

        //while(runCount != 0) {
        //    //unsigned int count = InterlockedExchangeAdd(&runCount, 0);
        //    pthread_cond_wait(&condDone, &doneLock);
        //}
        //pthread_mutex_unlock(&doneLock);

    EXIT :
        destroyThreadPool(workerpool, numcpu);
        destroyChessboardStatePool(boardpool, numcpu);
        printf("\nTotal number of solutions : %d \n\n", totalSolution);
    }
    endTime = OgreTimerGetMicroseconds(&timer);
	//executionTime = get_counter();
	printf("referenced execution time : %lf msec\n", (float)endTime / 1000.f);
	return 0;
}



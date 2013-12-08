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

typedef char bit8_field;

typedef struct _chessboard_status_t {
    unsigned int size;       // size = n
    bit8_field *column;    // n
    bit8_field *right;     // 2n - 1
    bit8_field *left;      // 2n - 1
    bit8_field *chessboard;// nxn
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

/*void markQueen(chessboard_status_t *board, const int row, const int col) {
    const int size = board->size;
    const int rightidx = col - row + size - 1;
    const int leftidx = col + row;
    char *grid = &board->chessboard[row * size + col];

    assert(board->column[col] && board->right[rightidx] &&
           board->left[leftidx] &&
           *grid == 'x');

    board->column[col] = board->right[rightidx] = board->left[leftidx] = 0;
    *grid = 'Q';
}*/

int bitCheck(bit8_field byte, int pos) {
    assert(pos < 8 && pos >= 0);
    return (1 << pos) & byte;
}

void bitSet(bit8_field *byte, int pos) {
    assert(pos < 8 && pos >= 0);
    *byte |= (1 << pos);
}

void bitClear(bit8_field *byte, int pos) {
    assert(pos < 8 && pos >= 0);
    *byte &= ~(1 << pos);
}

int tryMarkQueen(chessboard_status_t *board, const int row, const int col) {
    const int size = board->size;
    const int rightidx = col - row + size - 1;
    const int leftidx = col + row;
    const int grididx = row * size + col;

    bit8_field *colbit = &board->column[col/8];
    bit8_field *rbit = &board->right[rightidx/8];
    bit8_field *lbit = &board->left[leftidx/8];
    bit8_field *grid = &board->chessboard[grididx/8];

    if (bitCheck(*colbit, col % 8) &&
        bitCheck(*rbit, rightidx % 8) &&
        bitCheck(*lbit, leftidx % 8))
    {
        assert(0 != bitCheck(*grid, grididx % 8));

        bitClear(colbit, col % 8);
        bitClear(rbit, rightidx % 8);
        bitClear(lbit, leftidx % 8);
        bitClear(grid, grididx % 8);

        return 1;
    }
    return 0;
}

void unmarkQueen(chessboard_status_t *board, const int row, const int col) {
    const int size = board->size;
    const int rightidx = col - row + size - 1;
    const int leftidx = col + row;
    const int grididx = row * size + col;

    bit8_field *colbit = &board->column[col/8];
    bit8_field *rbit = &board->right[rightidx/8];
    bit8_field *lbit = &board->left[leftidx/8];
    bit8_field *grid = &board->chessboard[grididx/8];

    assert(0 == bitCheck(*grid, grididx % 8) &&
           0 == bitCheck(*colbit, col % 8) &&
           0 == bitCheck(*rbit, rightidx % 8) &&
           0 == bitCheck(*lbit, leftidx % 8));

    bitSet(colbit, col % 8);
    bitSet(rbit, rightidx % 8);
    bitSet(lbit, leftidx % 8);
    bitSet(grid, grididx % 8);
}

void printChessboard(const chessboard_status_t *board) {
    int i, j;
    const int size = board->size;

    printf("\n");
    for(i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            bit8_field *data = &board->chessboard[(i*size) / 8];
            if (bitCheck(*data, (i*size) % 8))
                printf("x");
            else
                printf("Q");
            //printf("%c", board->chessboard[i*size + j]);
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
    const int reqColumnSize = (size + (8 - 1))/8;
    const int reqDiagonalSize = ((2 * size - 1) + (8 - 1))/8;
    const int reqChessboardSize = (size * size + (8 - 1))/8;

    board->size = size;
    board->column = (bit8_field *)malloc(reqColumnSize);
    board->right = (bit8_field *)malloc(reqDiagonalSize);
    board->left = (bit8_field *)malloc(reqDiagonalSize);
    board->chessboard = (bit8_field *)malloc(reqChessboardSize);

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
    //int i;
    const int size = board->size;
    const int reqColumnSize = (size + (8 - 1))/8;
    const int reqDiagonalSize = ((2 * size - 1) + (8 - 1))/8;
    const int reqChessboardSize = (size * size + (8 - 1))/8;

    memset(board->chessboard, 0xFF, reqChessboardSize);
    memset(board->right, 0xFF, reqDiagonalSize);
    memset(board->left, 0xFF, reqDiagonalSize);
    memset(board->column, 0xFF, reqColumnSize);
    /*for (i = 0; i < 2 * size - 1; i++) {
        board->right[i] = 1;
        board->left[i] = 1;
    }

    for (i = 0; i < size; i++) {
        board->column[i] = 1;
    }*/
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
            //markQueen(worker->board, 0, curCol);
            tryMarkQueen(worker->board, 0, curCol);

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
            //int j = i - curRow + n - 1;
            //int k = i + curRow;
			//left and right are arrays for special process
            if (tryMarkQueen(board, curRow, i)) {
                //Mark Queens and recursive

                //char *grid = &board->chessboard[curRow * n + i];
                //board->column[i] = board->right[j] = board->left[k] = 0;
                //*grid = 'Q';
                //markQueen(board, curRow, i);

                queenDFS(worker, n, curRow + 1);

                unmarkQueen(board, curRow, i);
                //board->column[i] = board->right[j] = board->left[k] = 1;
                //*grid = 'x';
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
    OgreTimer_t timer;
    unsigned long endTime = 0;

    if (c <= 1 || (nn = atoi(v[1])) <= 0) nn = 14;

    OgreTimerInit(&timer);

    {
        // variables to be shared & updated **between threads**
        unsigned int totalSolution = 0;
        unsigned int colToBeProcessed = 0;
        unsigned int runCount = 0;
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

        for (i = 0; i < numcpu; i++) {
            chessboard_status_t *board = &boardpool[i];
            worker_t *worker = &workerpool[i];

            worker->totalSolution = &totalSolution;
            worker->colToBeProcessed = &colToBeProcessed;

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

    EXIT :
        destroyThreadPool(workerpool, numcpu);
        destroyChessboardStatePool(boardpool, numcpu);
        printf("\nTotal number of solutions : %d \n\n", totalSolution);
    }
    endTime = OgreTimerGetMicroseconds(&timer);
	printf("referenced execution time : %lf msec\n", (float)endTime / 1000.f);
	return 0;
}



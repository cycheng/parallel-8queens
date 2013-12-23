# Parallel 8-queens

* Introduction:
    * http://en.wikipedia.org/wiki/Eight_queens_puzzle

* TODO :
    * Cleanup code ! And use all threads !
    * Estimating each thread's loading, i.e. how many path they traversed, I
      want to think how to do load balence between them
      (important!)

    * Use cmake !
    * detect number of CPUs

# Major Change
* 2013 Dec 15
    * reduce search space :
      put needed check column into queue, unnecessary to check all column every
      time.
      Original for loop count = 377901384 (n = 14)
      New = 27358552 (n = 14)
      => also optimizing memory usage : Efficient memory access

* 2013 Dec 8
    * Change performance measurment method, don't use "rdtsc" (x86 instruction)
      when we are multicore, reason ref :
      http://en.wikipedia.org/wiki/Time_Stamp_Counter
      or
      http://blog.csdn.net/solstice/article/details/5196544

# Licensing

### MIT License
http://opensource.org/licenses/MIT
* ./3rd-party/Ogre*
* ./*.c

# Performance Result :

* Test System 1 :
    * CPU : Intel Core i7-620M 2.66 GHz, 2 Core, 4 threads
    * OS : Windows7 64-bits Service Pack1
    * TOOLCHAIN : MSVC 2010
    * BUILD OPTION :
        Release /O2 /Ob2 /Oi /Ot /Oy /GL /GF /Gm- /EHsc /GS- /Gy /arch:SSE2
        /fp:precise /Zc:wchar_t /Zc:forScope /Fp

## Score :

      +------------------+-------------+--------------+--------------+
      |                  | Base Serial |multithreading|Best Serial[1]|
      +------------------+-------------+--------------+--------------+
      | n = 14           | 5480.577381 | 3549.335905  | 354.941830   |
      | Execution Cycles |             |              |              |
      +------------------+-------------+--------------+--------------+
      | n = 14           | 2034.200    | 1348.822     | 128.439      |
      | Exe Time (ms)    |             |              |              |
      +------------------+-------------+--------------+--------------+

      [1] ref : http://rosettacode.org/wiki/N-queens_problem#C

      +------------------+---------------+---------------+---------------+---------------+
      | OPT METHOD for   | double worker | memory opt    | new search    | Remove        |
      | multithreading   | threads       |               | algorithm     | unnecessary   |
      | version          |               |               |               | Code [3]      |
      +------------------+---------------+---------------+---------------+---------------+
      | n = 14           | 2927.982956   |               |               |               |
      | Execution Cycles |               |               |               |               |
      +------------------+---------------+---------------+---------------+---------------+
      | n = 14           | 1131.899      | 3318.048 [1]  | 338.616 [2]   | 332.604       |
      | Exe Time (ms)    |               |               |               |               |
      +------------------+---------------+---------------+---------------+---------------+

      [1] becomes worse due to complex array index
          What I learned : be careful of bit optimization, complex index should avoid
      [2] Algorithm comes from http://www.cl.cam.ac.uk/~mr10/backtrk.pdf
          Very beautiful algorithm !
      [3] Remove malloc, memset, .. => original code for "memory opt", but
          unnecessary for new algorithm

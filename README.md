# Parallel 8-queens

* Introduction:
    * http://en.wikipedia.org/wiki/Eight_queens_puzzle

* TODO :
    * Estimating each thread's loading, i.e. how many path they traversed, I
      want to think how to do load balence between them
    * Optimizing memory usage
    * Use cmake !

# Major Change
* 2013 Dec 8
    * Change performance measurment method, don't use "rdtsc" (x86 instruction)
      when we are multicore, reason ref :
      http://en.wikipedia.org/wiki/Time_Stamp_Counter
      or
      http://blog.csdn.net/solstice/article/details/5196544

# Licensing
### MIT License
http://opensource.org/licenses/MIT

# Performance Result :

* Test System 1 :
    * CPU : Intel Core i7-620M 2.66 GHz, 2 Core, 4 threads
    * OS : Windows7 64-bits Service Pack1
    * TOOLCHAIN : MSVC 2010
    * BUILD OPTION :
        Release /O2 /Ob2 /Oi /Ot /Oy /GL /GF /Gm- /EHsc /GS- /Gy /arch:SSE2
        /fp:precise /Zc:wchar_t /Zc:forScope /Fp
    * Score :
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

      +------------------+---------------+
      | OPT METHOD for   | double worker |
      | multithreading   | threads       |
      | version          |               |
      +------------------+---------------+
      | n = 14           | 2927.982956   |
      | Execution Cycles |               |
      +------------------+---------------+
      | n = 14           | 1131.899      |
      | Exe Time (ms)    |               |
      +------------------+---------------+

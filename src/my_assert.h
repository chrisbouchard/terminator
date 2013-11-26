/* assert.h
 *
 * Chris Bouchard
 * 21 Nov 2013
 *
 * This header is a very simple assertion library used by terminator to run
 * system calls concisely.
 *
 * Define the ASSERT_DEBUG macro to enable debugging output.
 */

#ifndef MY_ASSERT_H_INCLUDED
#define MY_ASSERT_H_INCLUDED

/* Internal macro to implement debugging. Prints the name of the program, and
 * possibly the source file name and line number. */
#ifdef ASSERT_DEBUG
    #define ASSERT_PRINT_PROGRAM_NAME(FP) \
        fprintf((FP), "%s: %s: %d: ", ASSERT_PROGRAM_NAME, __FILE__, __LINE__)
#else
    #define ASSERT_PRINT_PROGRAM_NAME(FP) \
        fprintf((FP), "%s: ", ASSERT_PROGRAM_NAME)
#endif

/* Assert that X is nonzero. If the assertion fails, print an error message
 * suitable for the current errno to standard error and exit with failure. */
#define ASSERT(X) \
do { \
    if (!(X)) { \
        ASSERT_PRINT_PROGRAM_NAME(stderr); \
        perror(NULL); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* Assert that X is nonzero. If the assertion fails, print MSG to standard
 * error and exit with failure. */
#define ASSERT_WITH_MESSAGE(X, MSG) \
do { \
    if (!(X)) { \
        ASSERT_PRINT_PROGRAM_NAME(stderr); \
        fprintf(stderr, "%s\n", MSG); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

/* Assert that X is nonnegative. Behaves as ASSERT if the assertion fails. */
#define ASSERT_NONNEG(X) \
    ASSERT((X) >= 0)

/* Assert that X is nonnegative. Behaves as ASSERT_WITH_MESSAGE if the
 * assertion fails. */
#define ASSERT_NONNEG_WITH_MESSAGE(X, MSG) \
    ASSERT_WITH_MESSAGE((X) >= 0, MSG)

/* Assert that X is nonnegative. Behaves as ASSERT if the assertion fails. */
#define ASSERT_ZERO(X) \
    ASSERT((X) == 0)

/* Assert that X is nonnegative. Behaves as ASSERT_WITH_MESSAGE if the
 * assertion fails. */
#define ASSERT_ZERO_WITH_MESSAGE(X, MSG) \
    ASSERT_WITH_MESSAGE((X) == 0, MSG)

/* Assert that X is nonnegative. Behaves as ASSERT if the assertion fails. */
#define ASSERT_NONZERO(X) \
    ASSERT((X) != 0)

/* Assert that X is nonnegative. Behaves as ASSERT_WITH_MESSAGE if the
 * assertion fails. */
#define ASSERT_NONZERO_WITH_MESSAGE(X, MSG) \
    ASSERT_WITH_MESSAGE((X) != 0, MSG)

#endif /* MY_ASSERT_H_INCLUDED */


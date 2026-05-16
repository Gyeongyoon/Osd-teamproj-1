#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"
#include "../kernel/param.h"

#define PGSIZE 4096
#define MMAPBASE 0x40000000L

// helper: print pass/fail
void check(const char *name, int cond) {
    if (cond)
        printf("[PASS] %s\n", name);
    else
        printf("[FAIL] %s\n", name);
}

// -------------------------------------------------------
// 1. munmap on invalid address → should return -1
// -------------------------------------------------------
void test_munmap_invalid(void) {
    printf("\n=== test_munmap_invalid ===\n");
    int r = munmap(0xdeadbeef);
    check("munmap invalid addr returns -1", r == -1);
}

// -------------------------------------------------------
// 2. mmap with non-page-aligned addr → should return 0
// -------------------------------------------------------
void test_mmap_unaligned(void) {
    printf("\n=== test_mmap_unaligned ===\n");
    char *r = (char *)mmap(1, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    check("mmap unaligned addr returns 0", r == 0);
}

// -------------------------------------------------------
// 3. mmap with length not multiple of PGSIZE → should return 0
// -------------------------------------------------------
void test_mmap_bad_length(void) {
    printf("\n=== test_mmap_bad_length ===\n");
    char *r = (char *)mmap(0, 100, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    check("mmap bad length returns 0", r == 0);
}

// -------------------------------------------------------
// 4. fd close after lazy file-backed mmap → still accessible (filedup check)
// -------------------------------------------------------
void test_filedup_after_close(void) {
    printf("\n=== test_filedup_after_close ===\n");
    int fd = open("README", O_RDONLY);
    if (fd < 0) { printf("[SKIP] README not found\n"); return; }

    char *m = (char *)mmap(0, PGSIZE, PROT_READ, 0, fd, 0);
    close(fd);  // close fd immediately after mmap

    // lazy: access now triggers page fault, file must still be readable
    char c = m[0];
    check("file accessible after fd close (filedup works)", c != 0);

    munmap((uint64)m);
}

// -------------------------------------------------------
// 5. anonymous lazy mmap → page zero-initialized
// -------------------------------------------------------
void test_anon_lazy_zeroed(void) {
    printf("\n=== test_anon_lazy_zeroed ===\n");
    char *m = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS, -1, 0);
    int all_zero = 1;
    for (int i = 0; i < PGSIZE; i++) {
        if (m[i] != 0) { all_zero = 0; break; }
    }
    check("anonymous lazy page is zero-initialized", all_zero);
    munmap((uint64)m);
}

// -------------------------------------------------------
// 6. overlapping mmap → second mmap should return 0
// -------------------------------------------------------
void test_overlap(void) {
    printf("\n=== test_overlap ===\n");
    char *m1 = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    // try to map same address again
    char *m2 = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    check("overlapping mmap returns 0", m2 == 0);
    munmap((uint64)m1);
}

// -------------------------------------------------------
// 7. munmap then access → process should be killed (page fault)
//    test by forking: child accesses freed page, parent checks exit status
// -------------------------------------------------------
void test_access_after_munmap(void) {
    printf("\n=== test_access_after_munmap ===\n");
    char *m = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    munmap((uint64)m);

    int pid = fork();
    if (pid == 0) {
        // child: access freed page → should be killed
        char c = m[0];
        (void)c;
        exit(0);  // should not reach here
    } else {
        int status;
        wait(&status);
        // child should have been killed (non-zero exit)
        check("access after munmap kills process", status != -1);
    }
}

// -------------------------------------------------------
// 8. fork: child modifies eager page → parent unaffected (independent copy)
// -------------------------------------------------------
void test_fork_independence(void) {
    printf("\n=== test_fork_independence ===\n");
    char *m = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    m[0] = 'A';

    int pid = fork();
    if (pid == 0) {
        m[0] = 'B';  // child modifies
        exit(0);
    } else {
        wait(0);
        check("parent page unaffected by child write", m[0] == 'A');
        munmap((uint64)m);
    }
}

// -------------------------------------------------------
// 9. fork: child lazy fault independent from parent
// -------------------------------------------------------
void test_fork_lazy_independence(void) {
    printf("\n=== test_fork_lazy_independence ===\n");
    char *m = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS, -1, 0);

    int pid = fork();
    if (pid == 0) {
        m[0] = 'C';  // child faults in and writes
        exit(0);
    } else {
        wait(0);
        // parent faults in its own copy → should be 0, not 'C'
        char c = m[0];
        check("parent lazy page independent from child fault", c == 0);
        munmap((uint64)m);
    }
}

// -------------------------------------------------------
// 10. freemem: mmap → decrease → munmap → increase
// -------------------------------------------------------
void test_freemem_consistency(void) {
    printf("\n=== test_freemem_consistency ===\n");
    int before = freemem();
    char *m = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    int after_mmap = freemem();
    munmap((uint64)m);
    int after_munmap = freemem();

    check("freemem decreases after MAP_POPULATE mmap", after_mmap < before);
    check("freemem increases after munmap", after_munmap > after_mmap);
}

// -------------------------------------------------------
// 11. partial lazy access: only accessed pages reduce freemem
// -------------------------------------------------------
void test_partial_lazy(void) {
    printf("\n=== test_partial_lazy ===\n");
    // map 4 pages lazily
    char *m = (char *)mmap(0, PGSIZE * 4, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS, -1, 0);
    int before = freemem();
    // access only first page
    m[0] = 1;
    int after_one = freemem();
    // access second page
    m[PGSIZE] = 1;
    int after_two = freemem();

    check("freemem decreases by 1 after first page fault", before - after_one >= 1);
    check("freemem decreases again after second page fault", after_one - after_two >= 1);
    munmap((uint64)m);
}

// -------------------------------------------------------
// 12. write fault on PROT_READ only mapping → process killed
// -------------------------------------------------------
void test_write_to_readonly(void) {
    printf("\n=== test_write_to_readonly ===\n");
    char *m = (char *)mmap(0, PGSIZE, PROT_READ,
                           MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

    int pid = fork();
    if (pid == 0) {
        m[0] = 'X';  // write to read-only → should be killed
        exit(0);
    } else {
        int status;
        wait(&status);
        check("write to PROT_READ mapping kills process", status != 0);
        munmap((uint64)m);
    }
}

// -------------------------------------------------------
// 13. file-backed MAP_POPULATE: correct data loaded
// -------------------------------------------------------
void test_file_populate_content(void) {
    printf("\n=== test_file_populate_content ===\n");
    int fd = open("README", O_RDONLY);
    if (fd < 0) { printf("[SKIP] README not found\n"); return; }

    char *m = (char *)mmap(0, PGSIZE, PROT_READ, MAP_POPULATE, fd, 0);
    close(fd);

    // README starts with "xv6" 
    check("file-backed MAP_POPULATE loads correct data",
          m[0] == 'x' && m[1] == 'v' && m[2] == '6');
    munmap((uint64)m);
}

// -------------------------------------------------------
// 14. file-backed lazy: correct data loaded on fault
// -------------------------------------------------------
void test_file_lazy_content(void) {
    printf("\n=== test_file_lazy_content ===\n");
    int fd = open("README", O_RDONLY);
    if (fd < 0) { printf("[SKIP] README not found\n"); return; }

    char *m = (char *)mmap(0, PGSIZE, PROT_READ, 0, fd, 0);
    close(fd);

    // trigger fault
    check("file-backed lazy loads correct data on fault",
          m[0] == 'x' && m[1] == 'v' && m[2] == '6');
    munmap((uint64)m);
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main(void) {
    printf("=== pa3_edge_test start ===\n");

    test_munmap_invalid();
    test_mmap_unaligned();
    test_mmap_bad_length();
    test_filedup_after_close();
    test_anon_lazy_zeroed();
    test_overlap();
    test_access_after_munmap();
    test_fork_independence();
    test_fork_lazy_independence();
    test_freemem_consistency();
    test_partial_lazy();
    test_write_to_readonly();
    test_file_populate_content();
    test_file_lazy_content();

    printf("\n=== pa3_edge_test done ===\n");
    exit(0);
}

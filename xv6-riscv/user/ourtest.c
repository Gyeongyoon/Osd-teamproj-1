#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int passed = 0;
int failed = 0;

void check(const char *test, int actual, int expected)
{
  if (actual == expected) {
    printf("[PASS] %s : got %d\n", test, actual);
    passed++;
  } else {
    printf("[FAIL] %s : expected %d, got %d\n", test, expected, actual);
    failed++;
  }
}

int main()
{
  int ret;

  // ============================================================
  printf("========== Testing getnice() ==========\n");
  // ============================================================

  // pid 3 (mytest2) exists, default nice = 20
  ret = getnice(3);
  check("getnice(3) - mytest2 default nice", ret, 20);

  // set to min boundary (0) and verify
  setnice(1, 0);
  ret = getnice(1);
  check("getnice(1) - after setnice to 0 (min)", ret, 0);

  // set to max boundary (39) and verify
  setnice(1, 39);
  ret = getnice(1);
  check("getnice(1) - after setnice to 39 (max)", ret, 39);

  // restore to default
  setnice(1, 20);

  // pid 0 does not exist → should return -1
  ret = getnice(0);
  check("getnice(0) - no such process", ret, -1);

  // negative pid does not exist → should return -1
  ret = getnice(-1);
  check("getnice(-1) - negative pid", ret, -1);

  printf("\n");

  // ============================================================
  printf("========== Testing setnice() ==========\n");
  // ============================================================

  // min boundary 0 → should succeed
  ret = setnice(1, 0);
  check("setnice(1, 0) - min boundary", ret, 0);

  // max boundary 39 → should succeed
  ret = setnice(1, 39);
  check("setnice(1, 39) - max boundary", ret, 0);

  // just above max (40) → should fail
  ret = setnice(1, 40);
  check("setnice(1, 40) - just above max", ret, -1);

  // negative nice value → should fail
  ret = setnice(2, -1);
  check("setnice(2, -1) - negative nice value", ret, -1);

  // pid 0 does not exist → should fail
  ret = setnice(0, 10);
  check("setnice(0, 10) - no such process", ret, -1);

  // negative pid does not exist → should fail
  ret = setnice(-1, 10);
  check("setnice(-1, 10) - negative pid", ret, -1);

  // verify nice value change with getnice
  setnice(2, 5);
  ret = getnice(2);
  check("getnice(2) - after setnice to 5", ret, 5);

  // restore to default
  setnice(1, 20);
  setnice(2, 20);

  printf("\n");

  // ============================================================
  printf("========== Testing ps() ==========\n");
  // ============================================================

  // change nice values and verify they are reflected in ps
  setnice(1, 5);
  setnice(2, 15);
  printf("[INFO] ps(0) - init(nice=5), sh(nice=15) should be reflected:\n");
  ps(0);
  printf("\n");

  // restore to default
  setnice(1, 20);
  setnice(2, 20);

  // ps(2) → should print only sh
  printf("[INFO] ps(2) - should print only sh:\n");
  ps(2);
  printf("\n");

  // ps(3) → should print only mytest2
  printf("[INFO] ps(3) - should print only mytest2:\n");
  ps(3);
  printf("\n");

  // ps(99) → no such process, should print nothing
  printf("[INFO] ps(99) - no such process, should print nothing:\n");
  ps(99);
  printf("[INFO] (no output expected above)\n");
  printf("\n");

  // ============================================================
  printf("========== Testing meminfo() ==========\n");
  // ============================================================

  // call meminfo twice and check consistency
  uint64 mem1 = meminfo();
  uint64 mem2 = meminfo();
  if (mem1 == mem2) {
    printf("[PASS] meminfo() - consistent: %lu bytes\n", mem1);
    passed++;
  } else {
    printf("[FAIL] meminfo() - inconsistent: %lu vs %lu\n", mem1, mem2);
    failed++;
  }

  // check if return value is a multiple of PGSIZE(4096)
  if (mem1 % 4096 == 0) {
    printf("[PASS] meminfo() - multiple of PGSIZE(4096): %lu bytes\n", mem1);
    passed++;
  } else {
    printf("[FAIL] meminfo() - not a multiple of PGSIZE: %lu bytes\n", mem1);
    failed++;
  }

  printf("\n");

  // ============================================================
  printf("========== Testing waitpid() ==========\n");
  // ============================================================

  // fork a child and waitpid for it → should return 0
  int pid3 = fork();
  if (pid3 < 0) {
    printf("[FAIL] fork() failed\n");
    failed++;
  } else if (pid3 == 0) {
    exit(0);
  } else {
    ret = waitpid(pid3);
    check("waitpid(pid3) - child exited", ret, 0);
  }

  // wait for sh(pid 2) → not our child, should return -1
  ret = waitpid(2);
  check("waitpid(2) - not our child (sh)", ret, -1);

  // pid 0 does not exist → should return -1
  ret = waitpid(0);
  check("waitpid(0) - no such process", ret, -1);

  // negative pid does not exist → should return -1
  ret = waitpid(-1);
  check("waitpid(-1) - negative pid", ret, -1);

  printf("\n");

  // ============================================================
  printf("========== Summary ==========\n");
  // ============================================================
  printf("Total: %d passed, %d failed\n", passed, failed);

  exit(0);
}